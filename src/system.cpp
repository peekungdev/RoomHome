#include "system.h"
#include "lang.h"
#include <wlanapi.h>
#include <iphlpapi.h>
#include <netlistmgr.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>

NetMonitor g_net;
AudioCtl g_audio;

// ======================================================================
// helpers
// ======================================================================
static std::wstring EnvVar(const wchar_t* name) {
    wchar_t b[1024];
    DWORD k = GetEnvironmentVariableW(name, b, 1024);
    return (k && k < 1024) ? std::wstring(b) : std::wstring();
}

static std::wstring ProgramFiles64() {
    std::wstring p = EnvVar(L"ProgramW6432");
    return p.empty() ? EnvVar(L"ProgramFiles") : p;
}

static std::wstring ProgramFiles86() {
    std::wstring p = EnvVar(L"ProgramFiles(x86)");
    return p.empty() ? EnvVar(L"ProgramFiles") : p;
}

static std::wstring Unquote(std::wstring s) {
    s = Trim(s);
    if (s.size() >= 2 && s.front() == L'"') {
        size_t e = s.find(L'"', 1);
        if (e != std::wstring::npos) return s.substr(1, e - 1);
    }
    return s;
}

static std::wstring RegString(HKEY root, const std::wstring& sub, const wchar_t* value, REGSAM view) {
    HKEY k;
    if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_READ | view, &k) != ERROR_SUCCESS) return {};
    std::wstring out;
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(k, value, nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ) && size > 0 && size < 65536) {
        std::wstring buf(size / sizeof(wchar_t) + 1, L'\0');
        if (RegQueryValueExW(k, value, nullptr, &type, (BYTE*)buf.data(), &size) == ERROR_SUCCESS) {
            buf.resize(wcslen(buf.c_str()));
            out = type == REG_EXPAND_SZ ? ExpandEnv(buf) : buf;
        }
    }
    RegCloseKey(k);
    return out;
}

static std::wstring AppPathLookup(const std::wstring& exe) {
    std::wstring sub = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exe;
    const HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    const REGSAM views[] = { KEY_WOW64_64KEY, KEY_WOW64_32KEY };
    for (HKEY r : roots)
        for (REGSAM v : views) {
            std::wstring p = Unquote(RegString(r, sub, nullptr, v));
            if (!p.empty() && FileExists(p)) return p;
        }
    return {};
}

static std::wstring ProtocolExe(const wchar_t* scheme, const wchar_t* mustContain) {
    wchar_t buf[MAX_PATH * 2] = L"";
    DWORD n = MAX_PATH * 2;
    if (FAILED(AssocQueryStringW(ASSOCF_IS_PROTOCOL, ASSOCSTR_EXECUTABLE, scheme, L"open", buf, &n))) return {};
    std::wstring p = buf;
    if (!FileExists(p)) return {};
    if (mustContain && ToLower(p).find(mustContain) == std::wstring::npos) return {};
    return p;
}

static bool PackagedAppExists(const std::wstring& aumid) {
    ComPtr<IShellItem> it;
    std::wstring p = L"shell:AppsFolder\\" + aumid;
    return SUCCEEDED(SHCreateItemFromParsingName(p.c_str(), nullptr, IID_PPV_ARGS(&it)));
}

static std::wstring FirstExisting(std::initializer_list<std::wstring> paths) {
    for (auto& p : paths)
        if (!p.empty() && FileExists(p)) return p;
    return {};
}

std::wstring FindEdge() {
    std::wstring p = AppPathLookup(L"msedge.exe");
    if (!p.empty()) return p;
    return FirstExisting({ ProgramFiles86() + L"\\Microsoft\\Edge\\Application\\msedge.exe",
                           ProgramFiles64() + L"\\Microsoft\\Edge\\Application\\msedge.exe" });
}

static std::wstring FindChrome() {
    std::wstring p = AppPathLookup(L"chrome.exe");
    if (!p.empty()) return p;
    return FirstExisting({ ProgramFiles64() + L"\\Google\\Chrome\\Application\\chrome.exe",
                           ProgramFiles86() + L"\\Google\\Chrome\\Application\\chrome.exe",
                           EnvVar(L"LOCALAPPDATA") + L"\\Google\\Chrome\\Application\\chrome.exe" });
}

static std::wstring FindZoom() {
    std::wstring p = ProtocolExe(L"zoommtg", L"zoom");
    if (!p.empty()) return p;
    return FirstExisting({ EnvVar(L"APPDATA") + L"\\Zoom\\bin\\Zoom.exe",
                           ProgramFiles64() + L"\\Zoom\\bin\\Zoom.exe",
                           ProgramFiles86() + L"\\Zoom\\bin\\Zoom.exe" });
}

static std::wstring FindWebex() {
    std::wstring p = ProtocolExe(L"webexteams", L"cisco");
    if (p.empty()) p = ProtocolExe(L"ciscospark", L"cisco");
    if (!p.empty()) return p;
    return FirstExisting({ EnvVar(L"LOCALAPPDATA") + L"\\Programs\\Cisco Spark\\CiscoCollabHost.exe",
                           ProgramFiles64() + L"\\Cisco Spark\\CiscoCollabHost.exe",
                           ProgramFiles86() + L"\\Cisco Spark\\CiscoCollabHost.exe" });
}

static const wchar_t* kTeamsNew = L"MSTeams_8wekyb3d8bbwe!MSTeams";
static const wchar_t* kTeamsFree = L"MicrosoftTeams_8wekyb3d8bbwe!MicrosoftTeams";

static std::wstring ClassicTeamsUpdater() {
    return FirstExisting({ EnvVar(L"LOCALAPPDATA") + L"\\Microsoft\\Teams\\Update.exe" });
}

static std::wstring WindowsExe(const wchar_t* name) {
    wchar_t w[MAX_PATH] = L"";
    GetWindowsDirectoryW(w, MAX_PATH);
    return std::wstring(w) + L"\\" + name;
}

static std::wstring NormalizeUrl(std::wstring u) {
    u = Trim(u);
    u.erase(std::remove(u.begin(), u.end(), L'"'), u.end());
    if (u.empty()) return u;
    if (u.find(L"://") == std::wstring::npos && u.rfind(L"about:", 0) != 0 && u.rfind(L"edge:", 0) != 0 &&
        u.rfind(L"mailto:", 0) != 0)
        u = L"https://" + u;
    return u;
}

static bool ShellRun(const std::wstring& file, const std::wstring& args, std::wstring& err) {
    SHELLEXECUTEINFOW sei{ sizeof(sei) };
    sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"open";
    sei.lpFile = file.c_str();
    sei.lpParameters = args.empty() ? nullptr : args.c_str();
    std::wstring dir;
    if (file.find(L'\\') != std::wstring::npos && FileExists(file)) dir = file.substr(0, file.find_last_of(L'\\'));
    sei.lpDirectory = dir.empty() ? nullptr : dir.c_str();
    sei.nShow = SW_SHOWNORMAL;
    AllowSetForegroundWindow(ASFW_ANY);
    if (!ShellExecuteExW(&sei)) {
        err = HResultText(HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }
    if (sei.hProcess) {
        AllowSetForegroundWindow(GetProcessId(sei.hProcess));
        CloseHandle(sei.hProcess);
    }
    return true;
}

static bool OpenInEdge(const std::wstring& rawUrl, std::wstring& err) {
    std::wstring url = NormalizeUrl(rawUrl);
    std::wstring edge = FindEdge();
    if (!edge.empty()) {
        std::wstring args = L"--new-window";
        if (!url.empty()) args += L" \"" + url + L"\"";
        if (ShellRun(edge, args, err)) return true;
    }
    return ShellRun(L"microsoft-edge:" + url, L"", err);
}

static bool SendWinKey(WORD vk) {
    INPUT in[4] = {};
    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_LWIN;
    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = vk;
    in[2] = in[1]; in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3] = in[0]; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    return SendInput(4, in, sizeof(INPUT)) == 4;
}

static std::wstring ResolveProgram(const std::wstring& target) {
    std::wstring t = ExpandEnv(Unquote(target));
    if (t.empty()) return t;
    if (t.find(L'\\') == std::wstring::npos && t.find(L'/') == std::wstring::npos && t.find(L':') == std::wstring::npos) {
        std::wstring p = AppPathLookup(t);
        if (!p.empty()) return p;
        wchar_t buf[MAX_PATH];
        if (SearchPathW(nullptr, t.c_str(), nullptr, MAX_PATH, buf, nullptr)) return buf;
    }
    return t;
}

// ======================================================================
// launching
// ======================================================================
static void DoLaunch(const DockItem& it, LaunchMsg& m) {
    std::wstring err;
    const std::wstring& k = it.kind;
    auto done = [&](const std::wstring& text) { m.ok = true; m.text = text; };
    auto opening = [&](const std::wstring& name) { return Format(Tr(L"Opening %s"), Tr(name).c_str()); };
    auto onWeb = [&](const wchar_t* name) { return Format(Tr(L"%s isn't installed here, so it opened on the web"), name); };

    if (k == L"zoom") {
        std::wstring z = FindZoom();
        if (!z.empty()) { if (ShellRun(z, L"", err)) return done(opening(L"Zoom")); }
        else if (OpenInEdge(L"https://zoom.us/join", err)) return done(onWeb(L"Zoom"));
    } else if (k == L"teams") {
        if (PackagedAppExists(kTeamsNew)) { if (ShellRun(std::wstring(L"shell:AppsFolder\\") + kTeamsNew, L"", err)) return done(opening(L"Microsoft Teams")); }
        else if (PackagedAppExists(kTeamsFree)) { if (ShellRun(std::wstring(L"shell:AppsFolder\\") + kTeamsFree, L"", err)) return done(opening(L"Microsoft Teams")); }
        else if (!ClassicTeamsUpdater().empty()) { if (ShellRun(ClassicTeamsUpdater(), L"--processStart \"Teams.exe\"", err)) return done(opening(L"Microsoft Teams")); }
        else if (OpenInEdge(L"https://teams.microsoft.com", err)) return done(onWeb(L"Teams"));
    } else if (k == L"webex") {
        std::wstring w = FindWebex();
        if (!w.empty()) { if (ShellRun(w, L"", err)) return done(opening(L"Webex")); }
        else if (OpenInEdge(L"https://web.webex.com", err)) return done(onWeb(L"Webex"));
    } else if (k == L"web") {
        if (Trim(it.target).empty()) err = Tr(L"no web address is set");
        else if (OpenInEdge(it.target, err)) return done(opening(it.name));
    } else if (k == L"edge") {
        if (OpenInEdge(L"", err)) return done(opening(L"Microsoft Edge"));
    } else if (k == L"chrome") {
        std::wstring c = FindChrome();
        if (c.empty()) err = Tr(L"Google Chrome is not installed");
        else if (ShellRun(c, L"--new-window", err)) return done(opening(L"Google Chrome"));
    } else if (k == L"explorer") {
        if (ShellRun(WindowsExe(L"explorer.exe"), L"shell:MyComputerFolder", err)) return done(opening(L"File Explorer"));
    } else if (k == L"cast") {
        if (SendWinKey('K')) return done(Tr(L"Choose a display to cast to"));
        err = Tr(L"Windows blocked the shortcut");
    } else if (k == L"project") {
        if (SendWinKey('P')) return done(Tr(L"Choose a display mode"));
        err = Tr(L"Windows blocked the shortcut");
    } else {   // program
        std::wstring t = ExpandEnv(Trim(it.target));
        if (t.empty()) err = Tr(L"no program is set");
        else if (ShellRun(Unquote(t), it.args, err)) return done(opening(it.name));
    }
    m.ok = false;
    m.text = err.empty() ? Format(Tr(L"Couldn't open %s"), Tr(it.name).c_str())
                       : Format(Tr(L"Couldn't open %s: %s"), Tr(it.name).c_str(), err.c_str());
}

void LaunchAsync(const DockItem& item, HWND notify, bool dryRun) {
    std::thread([item, notify, dryRun] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        auto* m = new LaunchMsg;
        m->uid = item.uid;
        if (dryRun) { m->ok = true; m->text = Format(Tr(L"Test mode: %s"), DescribeLaunch(item).c_str()); }
        else DoLaunch(item, *m);
        if (!PostMessageW(notify, WM_APP_LAUNCHED, 0, (LPARAM)m)) delete m;
        CoUninitialize();
    }).detach();
}

std::wstring DescribeLaunch(const DockItem& it) {
    const std::wstring& k = it.kind;
    auto orWeb = [](const std::wstring& exe, const wchar_t* url) {
        return exe.empty() ? std::wstring(L"not installed -> Edge ") + url : exe;
    };
    if (k == L"zoom") return L"Zoom: " + orWeb(FindZoom(), L"https://zoom.us/join");
    if (k == L"teams") {
        if (PackagedAppExists(kTeamsNew)) return std::wstring(L"Teams: shell:AppsFolder\\") + kTeamsNew;
        if (PackagedAppExists(kTeamsFree)) return std::wstring(L"Teams: shell:AppsFolder\\") + kTeamsFree;
        if (!ClassicTeamsUpdater().empty()) return L"Teams (classic): " + ClassicTeamsUpdater();
        return L"Teams: not installed -> Edge https://teams.microsoft.com";
    }
    if (k == L"webex") return L"Webex: " + orWeb(FindWebex(), L"https://web.webex.com");
    if (k == L"web") return L"Edge (" + (FindEdge().empty() ? std::wstring(L"microsoft-edge: protocol") : FindEdge()) + L") " + NormalizeUrl(it.target);
    if (k == L"edge") return L"Edge: " + (FindEdge().empty() ? std::wstring(L"microsoft-edge: protocol") : FindEdge());
    if (k == L"chrome") return L"Chrome: " + (FindChrome().empty() ? std::wstring(L"not installed") : FindChrome());
    if (k == L"explorer") return WindowsExe(L"explorer.exe") + L" shell:MyComputerFolder";
    if (k == L"cast") return L"Win+K (Cast)";
    if (k == L"project") return L"Win+P (Project)";
    std::wstring r = ResolveProgram(it.target);
    return L"program: " + (r.empty() ? std::wstring(L"(none)") : r) + (it.args.empty() ? L"" : L" " + it.args);
}

std::wstring IconSourceFor(const DockItem& it) {
    const std::wstring& k = it.kind;
    if (k == L"zoom") return FindZoom();
    if (k == L"teams") {
        if (PackagedAppExists(kTeamsNew)) return std::wstring(L"shell:AppsFolder\\") + kTeamsNew;
        if (PackagedAppExists(kTeamsFree)) return std::wstring(L"shell:AppsFolder\\") + kTeamsFree;
        return {};
    }
    if (k == L"webex") return FindWebex();
    if (k == L"web" || k == L"edge") return FindEdge();
    if (k == L"chrome") return FindChrome();
    if (k == L"explorer") return WindowsExe(L"explorer.exe");
    if (k == L"program") return ResolveProgram(it.target);
    return {};
}

static void ExtractPixels(HBITMAP hb, IconMsg& m) {
    DIBSECTION ds{};
    int w = 0, h = 0;
    if (GetObjectW(hb, sizeof(ds), &ds) == sizeof(ds) && ds.dsBm.bmBits && ds.dsBm.bmBitsPixel == 32) {
        w = ds.dsBm.bmWidth; h = ds.dsBm.bmHeight;
        bool bottomUp = ds.dsBmih.biHeight > 0;
        m.px.resize((size_t)w * h * 4);
        for (int y = 0; y < h; y++) {
            int sy = bottomUp ? h - 1 - y : y;
            memcpy(&m.px[(size_t)y * w * 4], (BYTE*)ds.dsBm.bmBits + (size_t)sy * ds.dsBm.bmWidthBytes, (size_t)w * 4);
        }
    } else {
        BITMAP bm{};
        if (!GetObjectW(hb, sizeof(bm), &bm)) return;
        w = bm.bmWidth; h = bm.bmHeight;
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        m.px.resize((size_t)w * h * 4);
        HDC dc = GetDC(nullptr);
        int got = GetDIBits(dc, hb, 0, h, m.px.data(), &bi, DIB_RGB_COLORS);
        ReleaseDC(nullptr, dc);
        if (got != h) { m.px.clear(); return; }
    }
    bool anyAlpha = false, straight = false;
    for (size_t i = 0; i < m.px.size(); i += 4) {
        BYTE a = m.px[i + 3];
        if (a) anyAlpha = true;
        if (m.px[i] > a || m.px[i + 1] > a || m.px[i + 2] > a) straight = true;
    }
    for (size_t i = 0; i < m.px.size(); i += 4) {
        if (!anyAlpha) { m.px[i + 3] = 255; continue; }
        if (straight) {
            unsigned a = m.px[i + 3];
            m.px[i] = (BYTE)(m.px[i] * a / 255); m.px[i + 1] = (BYTE)(m.px[i + 1] * a / 255); m.px[i + 2] = (BYTE)(m.px[i + 2] * a / 255);
        }
    }
    m.w = w; m.h = h; m.ok = w > 0 && h > 0;
}

void LoadIconSync(const std::wstring& key, IconMsg& m) {
    m.key = key;
    ComPtr<IShellItemImageFactory> f;
    if (!key.empty() && SUCCEEDED(SHCreateItemFromParsingName(key.c_str(), nullptr, IID_PPV_ARGS(&f)))) {
        HBITMAP hb = nullptr;
        if (SUCCEEDED(f->GetImage(SIZE{ 256, 256 }, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &hb)) && hb) {
            ExtractPixels(hb, m);
            DeleteObject(hb);
        }
    }
}

void LoadIconAsync(const std::wstring& key, HWND notify) {
    std::thread([key, notify] {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        auto* m = new IconMsg;
        LoadIconSync(key, *m);
        if (!PostMessageW(notify, WM_APP_ICON, 0, (LPARAM)m)) delete m;
        CoUninitialize();
    }).detach();
}

// ======================================================================
// network
// ======================================================================
static std::wstring SsidToW(const DOT11_SSID& s) {
    if (!s.uSSIDLength) return {};
    const char* p = (const char*)s.ucSSID;
    int len = (int)std::min<ULONG>(s.uSSIDLength, 32);
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, len, nullptr, 0);
    UINT cp = CP_UTF8;
    DWORD fl = MB_ERR_INVALID_CHARS;
    if (n <= 0) { cp = CP_ACP; fl = 0; n = MultiByteToWideChar(cp, fl, p, len, nullptr, 0); }
    std::wstring w(n, L'\0');
    MultiByteToWideChar(cp, fl, p, len, w.data(), n);
    return w;
}

static std::wstring XmlEsc(const std::wstring& s) {
    std::wstring o;
    for (wchar_t c : s) {
        switch (c) {
        case L'&': o += L"&amp;"; break;
        case L'<': o += L"&lt;"; break;
        case L'>': o += L"&gt;"; break;
        case L'"': o += L"&quot;"; break;
        case L'\'': o += L"&apos;"; break;
        default: o += c;
        }
    }
    return o;
}

static std::wstring HexOf(const std::string& raw) {
    static const wchar_t* d = L"0123456789ABCDEF";
    std::wstring o;
    for (unsigned char c : raw) { o += d[c >> 4]; o += d[c & 15]; }
    return o;
}

static std::wstring ProfileXml(const std::wstring& name, const std::string& raw, const wchar_t* auth,
                               const wchar_t* enc, const std::wstring& key) {
    std::wstring x = L"<?xml version=\"1.0\"?>\r\n"
        L"<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">"
        L"<name>" + XmlEsc(name) + L"</name>"
        L"<SSIDConfig><SSID><hex>" + HexOf(raw) + L"</hex><name>" + XmlEsc(name) + L"</name></SSID></SSIDConfig>"
        L"<connectionType>ESS</connectionType><connectionMode>auto</connectionMode>"
        L"<MSM><security><authEncryption><authentication>" + std::wstring(auth) + L"</authentication>"
        L"<encryption>" + std::wstring(enc) + L"</encryption><useOneX>false</useOneX></authEncryption>";
    if (!key.empty())
        x += L"<sharedKey><keyType>passPhrase</keyType><protected>false</protected><keyMaterial>" + XmlEsc(key) +
             L"</keyMaterial></sharedKey>";
    x += L"</security></MSM></WLANProfile>";
    return x;
}

static std::wstring ReasonText(DWORD reason) {
    wchar_t b[512] = L"";
    WlanReasonCodeToString(reason, 512, b, nullptr);
    std::wstring s = Trim(b);
    while (!s.empty() && (s.back() == L'.')) s.pop_back();
    return s.empty() ? Format(L"reason %lu", reason) : s;
}

static std::wstring AddrStr(const SOCKADDR* sa) {
    wchar_t b[64] = L"";
    if (sa->sa_family == AF_INET) InetNtopW(AF_INET, &((const sockaddr_in*)sa)->sin_addr, b, 64);
    else if (sa->sa_family == AF_INET6) InetNtopW(AF_INET6, &((const sockaddr_in6*)sa)->sin6_addr, b, 64);
    return b;
}

static void FillIp(const IP_ADAPTER_ADDRESSES* a, std::wstring& ip, std::wstring& gw, std::wstring& dns) {
    std::wstring v6;
    for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
        if (u->Address.lpSockaddr->sa_family == AF_INET) { if (ip.empty()) ip = AddrStr(u->Address.lpSockaddr); }
        else if (v6.empty()) {
            auto* s6 = (const sockaddr_in6*)u->Address.lpSockaddr;
            if (!IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr)) v6 = AddrStr(u->Address.lpSockaddr);
        }
    }
    if (ip.empty()) ip = v6;
    for (auto* g = a->FirstGatewayAddress; g; g = g->Next) {
        if (g->Address.lpSockaddr->sa_family == AF_INET) { gw = AddrStr(g->Address.lpSockaddr); break; }
        if (gw.empty()) gw = AddrStr(g->Address.lpSockaddr);
    }
    int n = 0;
    for (auto* d = a->FirstDnsServerAddress; d && n < 2; d = d->Next) {
        if (d->Address.lpSockaddr->sa_family != AF_INET) continue;
        if (!dns.empty()) dns += L", ";
        dns += AddrStr(d->Address.lpSockaddr);
        n++;
    }
}

static std::wstring MacStr(const UCHAR* p, ULONG len) {
    std::wstring s;
    for (ULONG i = 0; i < len && i < 8; i++) s += Format(i ? L"-%02X" : L"%02X", p[i]);
    return s;
}

static VOID WINAPI WlanCallback(PWLAN_NOTIFICATION_DATA d, PVOID ctx) {
    static_cast<NetMonitor*>(ctx)->OnWlan(d);
}

void NetMonitor::Emit(WifiEvent::Code c, const std::wstring& text) {
    if (!notify_) return;
    auto* e = new WifiEvent{ c, text };
    if (!PostMessageW(notify_, WM_APP_WIFIEVENT, 0, (LPARAM)e)) delete e;
}

void NetMonitor::OnWlan(void* p) {
    auto* d = (PWLAN_NOTIFICATION_DATA)p;
    if (d && d->NotificationSource == WLAN_NOTIFICATION_SOURCE_ACM &&
        (d->NotificationCode == wlan_notification_acm_connection_complete ||
         d->NotificationCode == wlan_notification_acm_connection_attempt_fail) &&
        d->pData && d->dwDataSize >= sizeof(WLAN_CONNECTION_NOTIFICATION_DATA)) {
        auto* c = (PWLAN_CONNECTION_NOTIFICATION_DATA)d->pData;
        std::wstring profile = c->strProfileName;
        std::wstring ssid = SsidToW(c->dot11Ssid);
        bool ok = c->wlanReasonCode == WLAN_REASON_CODE_SUCCESS;
        std::wstring why = ok ? L"" : ReasonText(c->wlanReasonCode);
        bool emitOk = false, emitFail = false;
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!pendingProfile_.empty() && profile == pendingProfile_) {
                if (ok && d->NotificationCode == wlan_notification_acm_connection_complete) {
                    emitOk = true;
                    pendingProfile_.clear();
                } else if (!ok) {
                    emitFail = true;
                    if (pendingCreated_) {
                        std::wstring prof = pendingProfile_;
                        cmds_.push_back([this, prof](void* hv) {
                            if (hv && haveIf_) WlanDeleteProfile((HANDLE)hv, &ifGuid_, prof.c_str(), nullptr);
                        });
                    }
                    pendingProfile_.clear();
                }
            }
        }
        if (emitOk) Emit(WifiEvent::Connected, ssid.empty() ? profile : ssid);
        if (emitFail) Emit(WifiEvent::Failed, Format(Tr(L"Couldn't connect to %s: %s"), (ssid.empty() ? profile : ssid).c_str(), why.c_str()));
    }
    if (d && d->NotificationSource == WLAN_NOTIFICATION_SOURCE_ACM && d->NotificationCode == wlan_notification_acm_disconnected)
        Emit(WifiEvent::Disconnected, L"");
    if (wake_) SetEvent(wake_);
}

void NetMonitor::Start(HWND notify) {
    if (th_.joinable()) return;
    notify_ = notify;
    wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    first_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    th_ = std::thread([this] { Run(); });
}

void NetMonitor::Stop() {
    if (!th_.joinable()) return;
    stop_ = true;
    SetEvent(wake_);
    th_.join();
}

bool NetMonitor::WaitFirst(DWORD ms) { return first_ && WaitForSingleObject(first_, ms) == WAIT_OBJECT_0; }

NetInfo NetMonitor::Snapshot() {
    std::lock_guard<std::mutex> lk(mu_);
    return info_;
}

void NetMonitor::Post(std::function<void(void*)> cmd) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        cmds_.push_back(std::move(cmd));
    }
    if (wake_) SetEvent(wake_);
}

void NetMonitor::SetWantList(bool on) {
    bool was = wantList_.exchange(on);
    if (on && !was) { scanNow_ = true; if (wake_) SetEvent(wake_); }
}

void NetMonitor::Scan() {
    scanNow_ = true;
    if (wake_) SetEvent(wake_);
}

void NetMonitor::Connect(const WifiNet& net, const std::wstring& password) {
    Post([this, net, password](void* hv) {
        HANDLE h = (HANDLE)hv;
        if (!h || !haveIf_) { Emit(WifiEvent::ActionFailed, Tr(L"Wi-Fi is not available on this PC.")); return; }
        std::wstring profile;
        bool created = false;
        if (net.hasProfile && password.empty()) {
            profile = net.profile;
        } else {
            const wchar_t* auth = nullptr;
            const wchar_t* enc = L"AES";
            switch (net.auth) {
            case DOT11_AUTH_ALGO_80211_OPEN: if (!net.secure) { auth = L"open"; enc = L"none"; } break;
            case DOT11_AUTH_ALGO_WPA_PSK: auth = L"WPAPSK"; enc = net.cipher == DOT11_CIPHER_ALGO_TKIP ? L"TKIP" : L"AES"; break;
            case DOT11_AUTH_ALGO_RSNA_PSK: auth = L"WPA2PSK"; break;
            case DOT11_AUTH_ALGO_WPA3_SAE: auth = L"WPA3SAE"; break;
            default: break;
            }
            if (!auth) { Emit(WifiEvent::ActionFailed, Tr(L"This network needs a sign-in RoomHome can't do. Use the Windows Wi-Fi list instead.")); return; }
            if (net.secure && (password.size() < 8 || password.size() > 63)) {
                Emit(WifiEvent::ActionFailed, Tr(L"A Wi-Fi password has 8 to 63 characters."));
                return;
            }
            profile = net.ssid.empty() ? HexOf(net.ssidRaw) : net.ssid;
            std::wstring xml = ProfileXml(profile, net.ssidRaw, auth, enc, net.secure ? password : L"");
            DWORD reason = 0;
            DWORD e = WlanSetProfile(h, &ifGuid_, 0, xml.c_str(), nullptr, TRUE, nullptr, &reason);
            if (e == ERROR_ACCESS_DENIED)
                e = WlanSetProfile(h, &ifGuid_, WLAN_PROFILE_USER, xml.c_str(), nullptr, TRUE, nullptr, &reason);
            if (e != ERROR_SUCCESS) {
                Emit(WifiEvent::ActionFailed, Format(Tr(L"Couldn't save the network: %s"),
                     (reason ? ReasonText(reason) : HResultText(HRESULT_FROM_WIN32(e))).c_str()));
                return;
            }
            created = true;
        }
        DOT11_SSID ssid{};
        ssid.uSSIDLength = (ULONG)std::min<size_t>(net.ssidRaw.size(), DOT11_SSID_MAX_LENGTH);
        memcpy(ssid.ucSSID, net.ssidRaw.data(), ssid.uSSIDLength);
        WLAN_CONNECTION_PARAMETERS p{};
        p.wlanConnectionMode = wlan_connection_mode_profile;
        p.strProfile = profile.c_str();
        p.pDot11Ssid = ssid.uSSIDLength ? &ssid : nullptr;
        p.dot11BssType = dot11_BSS_type_infrastructure;
        {
            std::lock_guard<std::mutex> lk(mu_);
            pendingProfile_ = profile;
            pendingSsid_ = net.ssid;
            pendingCreated_ = created;
        }
        DWORD e = WlanConnect(h, &ifGuid_, &p, nullptr);
        if (e != ERROR_SUCCESS) {
            {
                std::lock_guard<std::mutex> lk(mu_);
                pendingProfile_.clear();
            }
            if (created) WlanDeleteProfile(h, &ifGuid_, profile.c_str(), nullptr);
            Emit(WifiEvent::Failed, Format(Tr(L"Couldn't connect: %s"), HResultText(HRESULT_FROM_WIN32(e)).c_str()));
            return;
        }
        Emit(WifiEvent::Info, Format(Tr(L"Connecting to %s\x2026"), net.ssid.c_str()));
    });
}

void NetMonitor::Disconnect() {
    Post([this](void* hv) {
        if (!hv || !haveIf_) return;
        DWORD e = WlanDisconnect((HANDLE)hv, &ifGuid_, nullptr);
        if (e != ERROR_SUCCESS) Emit(WifiEvent::ActionFailed, Format(Tr(L"Couldn't disconnect: %s"), HResultText(HRESULT_FROM_WIN32(e)).c_str()));
    });
}

void NetMonitor::Forget(const std::wstring& profile) {
    Post([this, profile](void* hv) {
        if (!hv || !haveIf_) return;
        DWORD e = WlanDeleteProfile((HANDLE)hv, &ifGuid_, profile.c_str(), nullptr);
        if (e == ERROR_SUCCESS) Emit(WifiEvent::Info, Format(Tr(L"Forgot %s"), profile.c_str()));
        else Emit(WifiEvent::ActionFailed, Format(Tr(L"Couldn't forget %s: %s"), profile.c_str(), HResultText(HRESULT_FROM_WIN32(e)).c_str()));
    });
}

void NetMonitor::SetRadio(bool on) {
    Post([this, on](void* hv) {
        HANDLE h = (HANDLE)hv;
        if (!h || !haveIf_) return;
        DWORD sz = 0;
        PVOID data = nullptr;
        WLAN_OPCODE_VALUE_TYPE vt;
        DWORD err = WlanQueryInterface(h, &ifGuid_, wlan_intf_opcode_radio_state, nullptr, &sz, &data, &vt);
        if (err == ERROR_SUCCESS && data) {
            auto* rs = (PWLAN_RADIO_STATE)data;
            for (DWORD i = 0; i < rs->dwNumberOfPhys && err == ERROR_SUCCESS; i++) {
                WLAN_PHY_RADIO_STATE s{};
                s.dwPhyIndex = rs->PhyRadioState[i].dwPhyIndex;
                s.dot11SoftwareRadioState = on ? dot11_radio_state_on : dot11_radio_state_off;
                err = WlanSetInterface(h, &ifGuid_, wlan_intf_opcode_radio_state, sizeof(s), &s, nullptr);
            }
            WlanFreeMemory(data);
        }
        if (err != ERROR_SUCCESS)
            Emit(WifiEvent::ActionFailed, Format(Tr(on ? L"Couldn't switch Wi-Fi on: %s" : L"Couldn't switch Wi-Fi off: %s"),
                 HResultText(HRESULT_FROM_WIN32(err)).c_str()));
    });
}

void NetMonitor::Gather(NetInfo& n) {
    HANDLE h = (HANDLE)wlan_;
    n.wlanService = h != nullptr;
    if (h) {
        PWLAN_INTERFACE_INFO_LIST list = nullptr;
        if (WlanEnumInterfaces(h, nullptr, &list) == ERROR_SUCCESS && list) {
            int pick = -1;
            for (DWORD i = 0; i < list->dwNumberOfItems; i++)
                if (pick < 0 || list->InterfaceInfo[i].isState == wlan_interface_state_connected) pick = (int)i;
            if (pick >= 0) {
                auto& ii = list->InterfaceInfo[pick];
                ifGuid_ = ii.InterfaceGuid;
                haveIf_ = true;
                n.wifiPresent = true;
                n.wifiAdapter = ii.strInterfaceDescription;
                n.wifiConnected = ii.isState == wlan_interface_state_connected;
                n.wifiConnecting = ii.isState == wlan_interface_state_associating ||
                                   ii.isState == wlan_interface_state_authenticating;
                DWORD sz = 0;
                PVOID data = nullptr;
                WLAN_OPCODE_VALUE_TYPE vt;
                if (WlanQueryInterface(h, &ifGuid_, wlan_intf_opcode_radio_state, nullptr, &sz, &data, &vt) == ERROR_SUCCESS && data) {
                    auto* rs = (PWLAN_RADIO_STATE)data;
                    bool on = rs->dwNumberOfPhys == 0;
                    for (DWORD k = 0; k < rs->dwNumberOfPhys; k++)
                        if (rs->PhyRadioState[k].dot11SoftwareRadioState == dot11_radio_state_on &&
                            rs->PhyRadioState[k].dot11HardwareRadioState == dot11_radio_state_on) on = true;
                    n.wifiRadioOn = on;
                    WlanFreeMemory(data);
                }
                if (n.wifiConnected) {
                    data = nullptr;
                    DWORD e = WlanQueryInterface(h, &ifGuid_, wlan_intf_opcode_current_connection, nullptr, &sz, &data, &vt);
                    if (e == ERROR_SUCCESS && data) {
                        auto* ca = (PWLAN_CONNECTION_ATTRIBUTES)data;
                        n.wifiSsid = SsidToW(ca->wlanAssociationAttributes.dot11Ssid);
                        n.wifiQuality = (int)ca->wlanAssociationAttributes.wlanSignalQuality;
                        WlanFreeMemory(data);
                    } else {
                        if (e == ERROR_ACCESS_DENIED) n.wifiDenied = true;
                        data = nullptr;
                        if (WlanQueryInterface(h, &ifGuid_, wlan_intf_opcode_rssi, nullptr, &sz, &data, &vt) == ERROR_SUCCESS && data) {
                            LONG rssi = *(LONG*)data;
                            n.wifiQuality = (int)Clamp(2.f * (rssi + 100), 0, 100);
                            WlanFreeMemory(data);
                        } else {
                            n.wifiQuality = 70;
                        }
                    }
                }
                if (wantList_) {
                    ULONGLONG t = GetTickCount64();
                    if (scanNow_.exchange(false) || t - lastScan_ > 20000) {
                        DWORD se = WlanScan(h, &ifGuid_, nullptr, nullptr, nullptr);
                        if (se == ERROR_ACCESS_DENIED) n.wifiDenied = true;
                        lastScan_ = t;
                    }
                    PWLAN_AVAILABLE_NETWORK_LIST al = nullptr;
                    DWORD e = WlanGetAvailableNetworkList(h, &ifGuid_, 0, nullptr, &al);
                    if (e == ERROR_SUCCESS && al) {
                        n.netsValid = true;
                        for (DWORD i = 0; i < al->dwNumberOfItems; i++) {
                            const WLAN_AVAILABLE_NETWORK& a = al->Network[i];
                            if (!a.dot11Ssid.uSSIDLength || a.dot11BssType != dot11_BSS_type_infrastructure) continue;
                            std::string raw((const char*)a.dot11Ssid.ucSSID, std::min<ULONG>(a.dot11Ssid.uSSIDLength, 32));
                            WifiNet* w = nullptr;
                            for (auto& x : n.nets) if (x.ssidRaw == raw) { w = &x; break; }
                            if (!w) {
                                n.nets.emplace_back();
                                w = &n.nets.back();
                                w->ssidRaw = raw;
                                w->ssid = SsidToW(a.dot11Ssid);
                                w->secure = a.bSecurityEnabled != FALSE;
                                w->auth = (int)a.dot11DefaultAuthAlgorithm;
                                w->cipher = (int)a.dot11DefaultCipherAlgorithm;
                                int au = w->auth;
                                w->supported = (au == DOT11_AUTH_ALGO_80211_OPEN && !w->secure) || au == DOT11_AUTH_ALGO_WPA_PSK ||
                                               au == DOT11_AUTH_ALGO_RSNA_PSK || au == DOT11_AUTH_ALGO_WPA3_SAE;
                            }
                            w->quality = std::max(w->quality, (int)a.wlanSignalQuality);
                            if (a.dwFlags & WLAN_AVAILABLE_NETWORK_CONNECTED) w->connected = true;
                            if ((a.dwFlags & WLAN_AVAILABLE_NETWORK_HAS_PROFILE) && a.strProfileName[0]) {
                                w->hasProfile = true;
                                w->profile = a.strProfileName;
                            }
                        }
                        WlanFreeMemory(al);
                        std::sort(n.nets.begin(), n.nets.end(), [](const WifiNet& a, const WifiNet& b) {
                            if (a.connected != b.connected) return a.connected;
                            if (a.hasProfile != b.hasProfile && std::abs(a.quality - b.quality) < 25) return a.hasProfile;
                            return a.quality > b.quality;
                        });
                    } else if (e == ERROR_ACCESS_DENIED) {
                        n.wifiDenied = true;
                    }
                }
            }
            WlanFreeMemory(list);
        }
    }

    // Ethernet + Wi-Fi addresses
    NET_LUID lanLuid{}, wifiLuid{};
    bool haveLan = false, haveWifi = false;
    PMIB_IF_TABLE2 tbl = nullptr;
    if (GetIfTable2(&tbl) == NO_ERROR && tbl) {
        for (ULONG i = 0; i < tbl->NumEntries; i++) {
            const MIB_IF_ROW2& r = tbl->Table[i];
            if (r.InterfaceAndOperStatusFlags.FilterInterface) continue;
            if (n.wifiPresent && IsEqualGUID(r.InterfaceGuid, ifGuid_)) {
                wifiLuid = r.InterfaceLuid;
                haveWifi = true;
                n.wifiMac = MacStr(r.PhysicalAddress, r.PhysicalAddressLength);
            }
            if (r.Type != IF_TYPE_ETHERNET_CSMACD || !r.InterfaceAndOperStatusFlags.HardwareInterface) continue;
            if (r.PhysicalMediumType != NdisPhysicalMedium802_3) continue;
            bool conn = r.MediaConnectState == MediaConnectStateConnected && r.OperStatus == IfOperStatusUp;
            if (!haveLan || (conn && !n.lanConnected)) {
                haveLan = true;
                lanLuid = r.InterfaceLuid;
                n.lanPresent = true;
                n.lanConnected = conn;
                n.lanSpeed = conn ? r.ReceiveLinkSpeed : 0;
                n.lanAdapter = r.Description;
                n.lanMac = MacStr(r.PhysicalAddress, r.PhysicalAddressLength);
            }
        }
        FreeMibTable(tbl);
    }
    if (haveLan || haveWifi) {
        ULONG size = 32768;
        std::vector<BYTE> buf(size);
        ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
        ULONG rc = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, (PIP_ADAPTER_ADDRESSES)buf.data(), &size);
        if (rc == ERROR_BUFFER_OVERFLOW) {
            buf.resize(size);
            rc = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, (PIP_ADAPTER_ADDRESSES)buf.data(), &size);
        }
        if (rc == NO_ERROR) {
            for (auto* a = (PIP_ADAPTER_ADDRESSES)buf.data(); a; a = a->Next) {
                if (haveLan && n.lanConnected && a->Luid.Value == lanLuid.Value) FillIp(a, n.lanIp, n.lanGateway, n.lanDns);
                if (haveWifi && n.wifiConnected && a->Luid.Value == wifiLuid.Value) FillIp(a, n.wifiIp, n.wifiGateway, n.wifiDns);
            }
        }
    }
}

void NetMonitor::Run() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<INetworkListManager> nlm;
    CoCreateInstance(CLSID_NetworkListManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&nlm));
    HANDLE h = nullptr;
    DWORD ver = 0;
    if (WlanOpenHandle(2, nullptr, &ver, &h) == ERROR_SUCCESS) {
        wlan_ = h;
        WlanRegisterNotification(h, WLAN_NOTIFICATION_SOURCE_ACM, TRUE, WlanCallback, this, nullptr, nullptr);
    }
    while (!stop_) {
        std::vector<std::function<void(void*)>> cmds;
        {
            std::lock_guard<std::mutex> lk(mu_);
            cmds.swap(cmds_);
        }
        for (auto& c : cmds) c(h);

        NetInfo n;
        Gather(n);
        if (nlm) {
            NLM_CONNECTIVITY c = NLM_CONNECTIVITY_DISCONNECTED;
            if (SUCCEEDED(nlm->GetConnectivity(&c))) {
                n.internet = (c & (NLM_CONNECTIVITY_IPV4_INTERNET | NLM_CONNECTIVITY_IPV6_INTERNET)) != 0;
                n.localOnly = !n.internet && (c & (NLM_CONNECTIVITY_IPV4_LOCALNETWORK | NLM_CONNECTIVITY_IPV4_SUBNET |
                                                   NLM_CONNECTIVITY_IPV6_LOCALNETWORK | NLM_CONNECTIVITY_IPV6_SUBNET)) != 0;
            }
            if (n.wifiConnected && n.wifiSsid.empty()) {   // no location consent: use the network profile name
                ComPtr<IEnumNetworkConnections> en;
                if (SUCCEEDED(nlm->GetNetworkConnections(&en)) && en) {
                    ComPtr<INetworkConnection> conn;
                    ULONG got = 0;
                    while (en->Next(1, conn.ReleaseAndGetAddressOf(), &got) == S_OK && got) {
                        GUID ad{};
                        if (SUCCEEDED(conn->GetAdapterId(&ad)) && IsEqualGUID(ad, ifGuid_)) {
                            ComPtr<INetwork> net;
                            BSTR name = nullptr;
                            if (SUCCEEDED(conn->GetNetwork(&net)) && net && SUCCEEDED(net->GetName(&name)) && name) {
                                n.wifiSsid = name;
                                SysFreeString(name);
                            }
                        }
                    }
                }
            }
        }
        {
            std::lock_guard<std::mutex> lk(mu_);
            n.seq = info_.seq + 1;
            info_ = std::move(n);
        }
        SetEvent(first_);
        if (notify_) PostMessageW(notify_, WM_APP_NET, 0, 0);
        WaitForSingleObject(wake_, wantList_ ? 3000 : 5000);
        Sleep(150);   // coalesce bursts of WLAN notifications
    }
    if (h) {
        WlanRegisterNotification(h, WLAN_NOTIFICATION_SOURCE_NONE, TRUE, nullptr, nullptr, nullptr, nullptr);
        WlanCloseHandle(h, nullptr);
        wlan_ = nullptr;
    }
    CoUninitialize();
}

// ======================================================================
// audio
// ======================================================================
static const PROPERTYKEY kFriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };

bool AudioCtl::Init(HWND notify) {
    hwnd_ = notify;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en_)))) return false;
    if (SUCCEEDED(en_->RegisterEndpointNotificationCallback(this))) registered_ = true;
    Reacquire();
    return Ok();
}

void AudioCtl::Reacquire() {
    if (vol_) { vol_->UnregisterControlChangeNotify(this); vol_.Reset(); }
    name_.clear();
    if (!en_) return;
    ComPtr<IMMDevice> dev;
    if (FAILED(en_->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) || !dev) return;
    if (FAILED(dev->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)vol_.ReleaseAndGetAddressOf()))) {
        vol_.Reset();
        return;
    }
    vol_->RegisterControlChangeNotify(this);
    ComPtr<IPropertyStore> ps;
    if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &ps))) {
        PROPVARIANT v;
        PropVariantInit(&v);
        if (SUCCEEDED(ps->GetValue(kFriendlyName, &v)) && v.vt == VT_LPWSTR && v.pwszVal) name_ = v.pwszVal;
        PropVariantClear(&v);
    }
}

void AudioCtl::Shutdown() {
    if (vol_) { vol_->UnregisterControlChangeNotify(this); vol_.Reset(); }
    if (en_ && registered_) en_->UnregisterEndpointNotificationCallback(this);
    registered_ = false;
    en_.Reset();
}

float AudioCtl::Level() {
    float v = 0;
    if (vol_) vol_->GetMasterVolumeLevelScalar(&v);
    return v;
}

bool AudioCtl::Muted() {
    BOOL m = FALSE;
    if (vol_) vol_->GetMute(&m);
    return m != FALSE;
}

void AudioCtl::SetLevel(float v) {
    if (!vol_) return;
    v = Saturate(v);
    vol_->SetMasterVolumeLevelScalar(v, nullptr);
    if (v > 0 && Muted()) vol_->SetMute(FALSE, nullptr);
}

void AudioCtl::Step(int dir, int percent) {
    if (!vol_) return;
    float cur = Level() * 100.f;
    float stepped = std::round(cur / percent) * percent + dir * percent;
    SetLevel(stepped / 100.f);
}

void AudioCtl::ToggleMute() {
    if (vol_) vol_->SetMute(Muted() ? FALSE : TRUE, nullptr);
}

STDMETHODIMP AudioCtl::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == __uuidof(IAudioEndpointVolumeCallback))
        *ppv = static_cast<IAudioEndpointVolumeCallback*>(this);
    else if (riid == __uuidof(IMMNotificationClient))
        *ppv = static_cast<IMMNotificationClient*>(this);
    else { *ppv = nullptr; return E_NOINTERFACE; }
    return S_OK;
}

STDMETHODIMP AudioCtl::OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA) {
    if (hwnd_) PostMessageW(hwnd_, WM_APP_AUDIO, 0, 0);
    return S_OK;
}

STDMETHODIMP AudioCtl::OnDeviceStateChanged(LPCWSTR, DWORD) {
    if (hwnd_) PostMessageW(hwnd_, WM_APP_AUDIODEV, 0, 0);
    return S_OK;
}

STDMETHODIMP AudioCtl::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) {
    if (flow == eRender && role == eConsole && hwnd_) PostMessageW(hwnd_, WM_APP_AUDIODEV, 0, 0);
    return S_OK;
}

// ======================================================================
// photos
// ======================================================================
bool IsImageFile(const std::wstring& name) {
    size_t dot = name.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring ext = ToLower(name.substr(dot + 1));
    for (const wchar_t* e : { L"jpg", L"jpeg", L"png", L"bmp", L"gif", L"tif", L"tiff", L"jxr", L"webp", L"heic", L"heif" })
        if (ext == e) return true;
    return false;
}

void LoadPhotoSync(const std::wstring& path, int tw, int th, PhotoMsg& m) {
    ComPtr<IWICImagingFactory> f;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f)))) return;
    ComPtr<IWICBitmapDecoder> dec;
    if (FAILED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec))) return;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(dec->GetFrame(0, &frame))) return;
    UINT iw = 0, ih = 0;
    frame->GetSize(&iw, &ih);
    if (!iw || !ih) return;

    // EXIF orientation (phone photos are often stored sideways)
    WICBitmapTransformOptions rot = WICBitmapTransformRotate0;
    ComPtr<IWICMetadataQueryReader> q;
    if (SUCCEEDED(frame->GetMetadataQueryReader(&q))) {
        PROPVARIANT v;
        PropVariantInit(&v);
        if (SUCCEEDED(q->GetMetadataByName(L"/app1/ifd/{ushort=274}", &v)) && v.vt == VT_UI2) {
            switch (v.uiVal) {
            case 2: rot = WICBitmapTransformFlipHorizontal; break;
            case 3: rot = WICBitmapTransformRotate180; break;
            case 4: rot = WICBitmapTransformFlipVertical; break;
            case 5: rot = (WICBitmapTransformOptions)(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal); break;
            case 6: rot = WICBitmapTransformRotate90; break;
            case 7: rot = (WICBitmapTransformOptions)(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal); break;
            case 8: rot = WICBitmapTransformRotate270; break;
            }
        }
        PropVariantClear(&v);
    }
    bool sideways = (rot & WICBitmapTransformRotate90) != 0;
    double ow = sideways ? ih : iw, oh = sideways ? iw : ih;   // upright size
    double s = std::max(tw / ow, th / oh);
    ComPtr<IWICBitmapSource> src = frame;
    if (s < 1.0) {
        ComPtr<IWICBitmapScaler> sc;
        if (SUCCEEDED(f->CreateBitmapScaler(&sc)) &&
            SUCCEEDED(sc->Initialize(frame.Get(), std::max(1u, (UINT)(iw * s + 0.5)), std::max(1u, (UINT)(ih * s + 0.5)),
                                     WICBitmapInterpolationModeHighQualityCubic)))
            src = sc;
    }
    if (rot != WICBitmapTransformRotate0) {
        ComPtr<IWICBitmapFlipRotator> fr;
        if (SUCCEEDED(f->CreateBitmapFlipRotator(&fr)) && SUCCEEDED(fr->Initialize(src.Get(), rot))) src = fr;
    }
    ComPtr<IWICFormatConverter> conv;
    if (FAILED(f->CreateFormatConverter(&conv)) ||
        FAILED(conv->Initialize(src.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                WICBitmapPaletteTypeMedianCut)))
        return;
    UINT w = 0, h = 0;
    conv->GetSize(&w, &h);
    if (!w || !h || (uint64_t)w * h > 40000000ULL) return;
    m.px.resize((size_t)w * h * 4);
    if (FAILED(conv->CopyPixels(nullptr, w * 4, (UINT)m.px.size(), m.px.data()))) { m.px.clear(); return; }
    m.w = (int)w;
    m.h = (int)h;
    m.ok = true;
}

void LoadPhotoAsync(const std::wstring& key, const std::wstring& path, int tw, int th, HWND notify) {
    std::thread([key, path, tw, th, notify] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        auto* m = new PhotoMsg;
        m->key = key;
        LoadPhotoSync(path, tw, th, *m);
        if (!PostMessageW(notify, WM_APP_PHOTO, 0, (LPARAM)m)) delete m;
        CoUninitialize();
    }).detach();
}

bool PickFolder(HWND owner, std::wstring& out) {
    ComPtr<IFileOpenDialog> d;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return false;
    FILEOPENDIALOGOPTIONS o = 0;
    d->GetOptions(&o);
    d->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    d->SetTitle(Tr(L"Choose a folder of pictures"));
    ComPtr<IShellItem> start;
    if (SUCCEEDED(SHGetKnownFolderItem(FOLDERID_Pictures, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&start)))) d->SetFolder(start.Get());
    if (d->Show(owner) != S_OK) return false;
    ComPtr<IShellItem> it;
    PWSTR p = nullptr;
    if (FAILED(d->GetResult(&it)) || FAILED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) return false;
    out = p;
    CoTaskMemFree(p);
    return true;
}

// ======================================================================
// time zones
// ======================================================================
static std::vector<ZoneInfo> s_zones;
static std::unordered_map<std::wstring, DYNAMIC_TIME_ZONE_INFORMATION> s_dtzi;

static void LoadZones() {
    if (!s_zones.empty()) return;
    DYNAMIC_TIME_ZONE_INFORMATION d{};
    for (DWORD i = 0; i < 1000 && EnumDynamicTimeZoneInformation(i, &d) == ERROR_SUCCESS; i++) {
        std::wstring key = d.TimeZoneKeyName;
        if (key.empty()) continue;
        std::wstring disp = RegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\" + key,
                                      L"Display", KEY_WOW64_64KEY);
        if (disp.empty()) disp = key;
        s_zones.push_back({ key, disp, -d.Bias });
        s_dtzi[key] = d;
    }
    std::stable_sort(s_zones.begin(), s_zones.end(), [](const ZoneInfo& a, const ZoneInfo& b) {
        if (a.offsetMin != b.offsetMin) return a.offsetMin < b.offsetMin;
        return a.display < b.display;
    });
}

const std::vector<ZoneInfo>& TimeZones() {
    LoadZones();
    return s_zones;
}

const ZoneInfo* FindZone(const std::wstring& key) {
    LoadZones();
    for (auto& z : s_zones) if (z.key == key) return &z;
    return nullptr;
}

static long long FileTime100ns(const SYSTEMTIME& st) {
    FILETIME ft{};
    SystemTimeToFileTime(&st, &ft);
    return ((long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

bool ZoneNow(const std::wstring& key, SYSTEMTIME& out, int* ahead) {
    LoadZones();
    auto it = s_dtzi.find(key);
    if (it == s_dtzi.end()) return false;
    SYSTEMTIME utc, local;
    GetSystemTime(&utc);
    if (!SystemTimeToTzSpecificLocalTimeEx(&it->second, &utc, &out)) return false;
    if (ahead) {
        SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local);   // same instant, this PC's zone
        long long d = FileTime100ns(out) - FileTime100ns(local);
        *ahead = (int)std::llround(d / 600000000.0);
    }
    return true;
}

// ======================================================================
// power, autostart, misc
// ======================================================================
bool PowerAction(bool restart, std::wstring& err) {
    HANDLE tok = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) {
        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid))
            AdjustTokenPrivileges(tok, FALSE, &tp, 0, nullptr, nullptr);
        CloseHandle(tok);
    }
    UINT flags = (restart ? EWX_REBOOT : (EWX_SHUTDOWN | EWX_POWEROFF)) | EWX_FORCEIFHUNG;
    if (!ExitWindowsEx(flags, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED)) {
        err = HResultText(HRESULT_FROM_WIN32(GetLastError()));
        return false;
    }
    return true;
}

static const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

static bool RunValueExists(HKEY root) {
    HKEY k;
    if (RegOpenKeyExW(root, kRunKey, 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return false;
    bool ok = RegQueryValueExW(k, APP_NAME, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey(k);
    return ok;
}

static bool SetRunValue(HKEY root, bool on) {
    HKEY k;
    LSTATUS s = RegCreateKeyExW(root, kRunKey, 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr);
    if (s != ERROR_SUCCESS) return false;
    if (on) {
        std::wstring cmd = L"\"" + ExePath() + L"\" --autostart";
        s = RegSetValueExW(k, APP_NAME, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        s = RegDeleteValueW(k, APP_NAME);
        if (s == ERROR_FILE_NOT_FOUND) s = ERROR_SUCCESS;
    }
    RegCloseKey(k);
    return s == ERROR_SUCCESS;
}

bool AutostartUser() { return RunValueExists(HKEY_CURRENT_USER); }
bool AutostartMachine() { return RunValueExists(HKEY_LOCAL_MACHINE); }
bool SetAutostartUser(bool on) { return SetRunValue(HKEY_CURRENT_USER, on); }
bool SetAutostartMachine(bool on) { return SetRunValue(HKEY_LOCAL_MACHINE, on); }

bool RunSelfElevated(const std::wstring& args) {
    std::wstring exe = ExePath();
    SHELLEXECUTEINFOW sei{ sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = exe.c_str();
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) return false;
    DWORD code = 1;
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 20000);
        GetExitCodeProcess(sei.hProcess, &code);
        CloseHandle(sei.hProcess);
    }
    return code == 0;
}

bool SystemPrefersLight() {
    DWORD v = 1, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) != ERROR_SUCCESS)
        return true;
    return v != 0;
}

bool OpenUri(const std::wstring& uri) {
    std::wstring err;
    return ShellRun(uri, L"", err);
}

bool PickFile(HWND owner, bool image, std::wstring& out) {
    ComPtr<IFileOpenDialog> d;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return false;
    COMDLG_FILTERSPEC img[] = { { Tr(L"Images"), L"*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tif;*.tiff;*.jxr;*.webp;*.heic" },
                                { Tr(L"All files"), L"*.*" } };
    COMDLG_FILTERSPEC prog[] = { { Tr(L"Programs and shortcuts"), L"*.exe;*.lnk;*.bat;*.cmd;*.url;*.appref-ms" },
                                 { Tr(L"All files"), L"*.*" } };
    d->SetFileTypes(2, image ? img : prog);
    FILEOPENDIALOGOPTIONS o = 0;
    d->GetOptions(&o);
    d->SetOptions(o | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | (image ? 0 : FOS_NODEREFERENCELINKS));
    d->SetTitle(Tr(image ? L"Choose a wallpaper" : L"Choose a program or shortcut"));
    ComPtr<IShellItem> start;
    if (SUCCEEDED(SHGetKnownFolderItem(image ? FOLDERID_Pictures : FOLDERID_CommonPrograms, KF_FLAG_DEFAULT, nullptr,
                                       IID_PPV_ARGS(&start))))
        d->SetFolder(start.Get());
    if (d->Show(owner) != S_OK) return false;
    ComPtr<IShellItem> it;
    PWSTR p = nullptr;
    if (FAILED(d->GetResult(&it)) || FAILED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) return false;
    out = p;
    CoTaskMemFree(p);
    return true;
}

std::wstring ComputerName() {
    wchar_t b[256];
    DWORD n = 256;
    return GetComputerNameExW(ComputerNamePhysicalDnsHostname, b, &n) ? std::wstring(b) : std::wstring(L"PC");
}

int LocaleFirstDayIsMonday() {
    wchar_t b[8] = L"";
    GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_IFIRSTDAYOFWEEK, b, 8);
    return b[0] == L'0' ? 1 : 0;
}

bool LocaleUses24h() {
    wchar_t b[80] = L"";
    GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, b, 80);
    return wcschr(b, L'H') != nullptr;
}
