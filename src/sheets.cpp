// Modal sheets: Wi-Fi, network details, settings, power, add / edit app, room name, admin PIN.
#include "app.h"
#include "lang.h"

static D2D1_MATRIX_3X2_F s_savedWorld;
static void StartCameraTest();

void OpenSheet(Sheet s) {
    if (g.sheet == Sheet::Camera && s != Sheet::Camera) g_camPreview.Stop();
    edits.Clear();
    if (g.sheetT < 0.02f) ui.AnimSet(Id("sheetT"), 0.f);
    g.sheet = s;
    g.sheetOpen = true;
    ui.ClearFocus();
    g_net.SetWantList(s == Sheet::Wifi);
    switch (s) {
    case Sheet::Wifi:
        g.wifi.selected.clear();
        g.wifi.password.clear();
        g.wifi.showPassword = false;
        g.wifi.scroll = 0;
        g.wifi.status.clear();
        g_net.Scan();
        break;
    case Sheet::Rename:
        g.renameTitle = g.cfg.roomName;
        g.renameSubtitle = g.cfg.subtitle;
        break;
    case Sheet::Settings:
        g.pinDraft = g.cfg.adminPin;
        RefreshAutostart();
        break;
    case Sheet::Pin:
        g.pin.entered.clear();
        g.pin.wrong = false;
        break;
    case Sheet::Camera:
        StartCameraTest();
        break;
    default:
        break;
    }
    RequestFrame();
}

static void CommitSettings() {
    std::wstring pin;
    for (wchar_t c : g.pinDraft) if (c >= L'0' && c <= L'9') pin += c;
    if (pin != g.cfg.adminPin) {
        if (!pin.empty() && pin.size() < 4) ShowToast(L"The admin PIN needs at least 4 digits, so it was not changed.", 2);
        else {
            g.cfg.adminPin = pin;
            ShowToast(pin.empty() ? L"Admin PIN removed" : L"Admin PIN set", 1);
        }
    }
    g.cfg.roomName = Trim(g.cfg.roomName);   // empty = the default name, in the current language
    g.cfg.subtitle = Trim(g.cfg.subtitle);
    SaveConfig();
}

void CloseSheet() {
    if (!g.sheetOpen) return;
    if (g.sheet == Sheet::Settings) CommitSettings();
    if (g.sheet == Sheet::Camera) {   // free the camera at once; the last picture stays for the closing fade
        g_camPreview.Stop();
        g.cam.paused = false;
    }
    edits.Clear();
    g.sheetOpen = false;
    g_net.SetWantList(false);
    ui.ClearFocus();
    if (g.hwnd) SetFocus(g.hwnd);
    RequestFrame();
}

void RequireUnlock(std::function<void()> then) {
    if (g.cfg.adminPin.empty() || g.now < g.unlockedUntil) { then(); return; }
    g.pin = PinUi();
    g.pin.then = std::move(then);
    OpenSheet(Sheet::Pin);
}

void SetTheme(int theme) {
    g.cfg.theme = theme;
    SaveConfig();
    RequestFrame();
}

// ---------------------------------------------------------------- frame
static Box BeginSheet(float w, float h, const std::wstring& title, const std::wstring& subtitle) {
    ui.layer = 1;
    float t = g.sheetT, e = EaseOutCubic(t);
    gfx.FillRect({ -40.f, -40.f, g.vw + 80.f, g.vh + 80.f }, MulA(g.th.dim, t));
    w = std::min(w, g.vw - 40.f);
    h = std::min(h, g.vh - 40.f);
    Box card{ std::round((g.vw - w) / 2), std::round((g.vh - h) / 2), w, h };
    ui.Add(Id("sheet.backdrop"), { -40.f, -40.f, g.vw + 80.f, g.vh + 80.f }, false);
    ui.Add(Id("sheet.card"), card, false);
    if (ui.Clicked(Id("sheet.backdrop")) && g.sheet != Sheet::Pin) Defer(CloseSheet);
    gfx.rt->GetTransform(&s_savedWorld);
    float sc = Lerp(0.94f, 1.f, e);
    gfx.rt->SetTransform(D2D1::Matrix3x2F::Scale(sc, sc, D2D1::Point2F(card.cx(), card.cy())) *
                         D2D1::Matrix3x2F::Translation(0, (1.f - e) * 28.f) * s_savedWorld);
    // A half-transparent card let the big clock show through its buttons (double exposure):
    // the card turns opaque within the first third of the animation and arrives by scale + rise.
    gfx.PushOpacity(Saturate(t * (g.sheetOpen ? 3.f : 2.f)));
    gfx.Shadow(card, 36.f, 60.f, Lerp(0.55f, 0.22f, g.th.light), 24.f, g.th.shadow);
    gfx.FillRect(card, g.th.sheet, 36.f);
    gfx.StrokeRect(card.inset(0.6f), MulA(g.th.glassStroke, 0.7f), 36.f, 1.2f);
    gfx.Text(title, { card.x + 44.f, card.y + 30.f, card.w - 220.f, 54.f }, FACE_DISPLAY, 38.f, W_SEMIBOLD, g.th.text);
    if (!subtitle.empty())
        gfx.Text(subtitle, { card.x + 44.f, card.y + 82.f, card.w - 140.f, 32.f }, FACE_TEXT, 21.f, W_REGULAR, g.th.text2);
    if (IconButton(Id("sheet.close"), { card.r() - 44.f - 56.f, card.y + 30.f, 56.f, 56.f }, 0xE711, BTN_GLASS, 20.f))
        Defer(CloseSheet);
    return card;
}

static void EndSheet() {
    gfx.PopOpacity();
    gfx.rt->SetTransform(s_savedWorld);
}

static void Label(const std::wstring& s, float x, float y, float w) {
    gfx.Text(s, { x, y, w, 30.f }, FACE_TEXT, 19.f, W_SEMIBOLD, g.th.text2, AL_LEFT);
}

static void Message(Box body, wchar_t glyph, const std::wstring& title, const std::wstring& text) {
    float cy = body.y + body.h * 0.30f;
    gfx.FillCircle(body.cx(), cy, 46.f, g.th.track);
    gfx.Glyph(glyph, { body.cx() - 46.f, cy - 46.f, 92.f, 92.f }, 38.f, g.th.text2);
    gfx.Text(title, { body.x, cy + 66.f, body.w, 44.f }, FACE_TEXT, 28.f, W_SEMIBOLD, g.th.text, AL_CENTER);
    if (!text.empty())
        gfx.Text(text, { body.x + 30.f, cy + 116.f, body.w - 60.f, 120.f }, FACE_TEXT, 21.f, W_REGULAR, g.th.text2, AL_CENTER, false, true);
}

// ---------------------------------------------------------------- Wi-Fi
static int Level(int q) { return q >= 75 ? 4 : q >= 50 ? 3 : q >= 25 ? 2 : 1; }

static void ConnectNet(const WifiNet& w) {
    if (w.secure && !w.hasProfile && g.wifi.password.empty()) {
        g.wifi.status = L"Type the Wi-Fi password first.";
        g.wifi.statusKind = 2;
        return;
    }
    g_net.Connect(w, w.hasProfile ? std::wstring() : g.wifi.password);
    g.wifi.status = Format(Tr(L"Connecting to %s\x2026"), w.ssid.c_str());
    g.wifi.statusKind = 0;
}

static const WifiNet* SelectedNet() {
    for (auto& w : g.net.nets) if (w.ssidRaw == g.wifi.selected) return &w;
    return nullptr;
}

static void WifiRow(const WifiNet& w, Box row, bool sel, float rowH) {
    uint64_t rid = Mix64(HashBytes(w.ssidRaw.data(), w.ssidRaw.size(), Id("wifi.row")));
    Box head{ row.x, row.y, row.w, rowH };
    ui.Add(rid, head);
    float hov = ui.Anim(rid ^ 0x51, ui.Highlighted(rid) ? 1.f : 0.f, 16.f);
    gfx.FillRect(row, Mix(g.th.glass, g.th.glassHi, sel ? 0.9f : hov * 0.7f), 22.f);
    if (sel) gfx.StrokeRect(row.inset(0.75f), MulA(g.th.accent, 0.8f), 22.f, 1.5f);
    DrawWifiIcon(row.x + 46.f, head.cy() - 3.f, 34.f, Level(w.quality), g.th.text, MulA(g.th.text3, 0.5f));
    if (w.secure) gfx.Glyph(0xE72E, { row.x + 56.f, head.cy() + 2.f, 24.f, 24.f }, 14.f, g.th.text2);
    gfx.Text(w.ssid.empty() ? L"(hidden network)" : w.ssid, { row.x + 92.f, head.y + 12.f, row.w - 92.f - 80.f, 34.f },
             FACE_TEXT, 25.f, W_SEMIBOLD, g.th.text);
    std::wstring sub = Tr(w.connected ? L"Connected" : w.hasProfile ? L"Saved" : w.secure ? L"Secured" : L"Open");
    if (!w.supported && !w.hasProfile) sub += Tr(L" \x00B7 needs Windows sign-in");
    sub += Format(L" \x00B7 %d%%", w.quality);
    gfx.Text(sub, { row.x + 92.f, head.y + 46.f, row.w - 92.f - 80.f, 26.f }, FACE_TEXT, 18.f, W_REGULAR,
             w.connected ? g.th.accent : g.th.text2);
    if (w.connected) gfx.Glyph(0xE73E, { row.r() - 70.f, head.y, 50.f, rowH }, 24.f, g.th.accent);
    FocusRing(rid, head, 22.f);
    if (ui.Clicked(rid)) {
        std::string raw = w.ssidRaw;
        Defer([raw, sel] {
            g.wifi.selected = sel ? std::string() : raw;
            g.wifi.password.clear();
            g.wifi.showPassword = false;
            edits.Clear();
        });
    }
    if (!sel) return;

    Box act{ row.x + 24.f, head.b() + 2.f, row.w - 48.f, row.h - rowH - 18.f };
    float bh = 60.f;
    float by = act.cy() - bh / 2;
    WifiNet copy = w;
    if (w.connected) {
        if (Button(Id("wifi.disc"), { act.r() - 200.f, by, 200.f, bh }, L"Disconnect", BTN_GLASS))
            Defer([] { g_net.Disconnect(); g.wifi.status = L"Disconnecting\x2026"; g.wifi.statusKind = 0; });
        if (w.hasProfile && Button(Id("wifi.forget"), { act.r() - 200.f - 16.f - 160.f, by, 160.f, bh }, L"Forget", BTN_PLAIN))
            Defer([copy] { g_net.Forget(copy.profile); });
    } else if (w.hasProfile) {
        if (Button(Id("wifi.conn"), { act.r() - 200.f, by, 200.f, bh }, L"Connect", BTN_ACCENT))
            Defer([copy] { ConnectNet(copy); });
        if (Button(Id("wifi.forget"), { act.r() - 200.f - 16.f - 160.f, by, 160.f, bh }, L"Forget", BTN_PLAIN))
            Defer([copy] { g_net.Forget(copy.profile); });
    } else if (!w.supported) {
        gfx.Text(L"Company networks need a Windows sign-in.", { act.x, by, act.w - 300.f, bh }, FACE_TEXT, 20.f, W_REGULAR, g.th.text2);
        if (Button(Id("wifi.winlist"), { act.r() - 280.f, by, 280.f, bh }, L"Windows Wi-Fi list", BTN_GLASS))
            Defer([] { OpenUri(L"ms-availablenetworks:"); });
    } else if (w.secure) {
        Box fb{ act.x, by, act.w - 200.f - 16.f - 64.f - 12.f, bh };
        Field(Id("wifi.pw"), fb, &g.wifi.password, L"Password", EF_PASSWORD | EF_AUTOFOCUS);
        if (IconButton(Id("wifi.eye"), { fb.r() + 12.f, by + 2.f, 56.f, 56.f }, g.wifi.showPassword ? (wchar_t)0xED1A : (wchar_t)0xE890,
                       BTN_PLAIN, 22.f))
            Defer([] { g.wifi.showPassword = !g.wifi.showPassword; edits.SetPasswordVisible(Id("wifi.pw"), g.wifi.showPassword); });
        if (Button(Id("wifi.conn"), { act.r() - 200.f, by, 200.f, bh }, L"Connect", BTN_ACCENT))
            Defer([copy] { ConnectNet(copy); });
    } else {
        gfx.Text(L"Open network: anyone nearby can read the traffic.", { act.x, by, act.w - 240.f, bh }, FACE_TEXT, 20.f,
                 W_REGULAR, g.th.text2);
        if (Button(Id("wifi.conn"), { act.r() - 200.f, by, 200.f, bh }, L"Connect", BTN_ACCENT))
            Defer([copy] { ConnectNet(copy); });
    }
}

static void SheetWifi() {
    NetInfo& n = g.net;
    std::wstring sub = !n.wifiPresent ? L"" : n.wifiConnected ? Format(Tr(L"Connected to %s"), n.wifiSsid.c_str())
                     : n.wifiConnecting ? L"Connecting\x2026" : L"Not connected";
    Box card = BeginSheet(820.f, 880.f, L"Wi-Fi", sub);
    float x = card.x + 44.f, w = card.w - 88.f;
    if (n.wifiPresent && n.wlanService) {
        Box tg{ card.r() - 44.f - 56.f - 24.f - 90.f, card.y + 38.f, 90.f, 44.f };
        if (Toggle(Id("wifi.radio"), tg, n.wifiRadioOn)) {
            bool on = !n.wifiRadioOn;
            Defer([on] { g_net.SetRadio(on); g.wifi.status = on ? L"Turning Wi-Fi on\x2026" : L"Turning Wi-Fi off\x2026"; g.wifi.statusKind = 0; });
        }
        if (n.wifiRadioOn &&
            IconButton(Id("wifi.scan"), { tg.x - 76.f, card.y + 30.f, 56.f, 56.f }, 0xE72C, BTN_GLASS, 20.f))
            Defer([] { g_net.Scan(); g.wifi.status = L"Looking for networks\x2026"; g.wifi.statusKind = 0; });
    }
    Box footer{ x, card.b() - 40.f - 60.f, w, 60.f };
    Box body{ x, card.y + 132.f, w, footer.y - 60.f - (card.y + 132.f) };

    if (!n.wlanService) {
        Message(body, 0xE7BA, L"The Wi-Fi service isn't running", L"Windows' WLAN AutoConfig service is stopped or not installed.");
    } else if (!n.wifiPresent) {
        Message(body, 0xE701, L"No Wi-Fi adapter", L"This PC has no wireless adapter. Use the LAN cable instead.");
    } else if (!n.wifiRadioOn) {
        Message(body, 0xE701, L"Wi-Fi is off", L"Switch it on to see the networks around this room.");
        if (Button(Id("wifi.on"), { body.cx() - 150.f, body.y + body.h * 0.30f + 250.f, 300.f, 62.f }, L"Turn on Wi-Fi", BTN_ACCENT))
            Defer([] { g_net.SetRadio(true); });
    } else if (n.wifiDenied) {
        Message(body, 0xE72E, L"Windows is hiding the network list",
                L"Windows 11 only lets apps list Wi-Fi networks when location access is on. In Settings, open "
                L"Privacy & security \x203A Location and turn on \x201CLet desktop apps access your location\x201D.");
        float by = body.y + body.h * 0.30f + 250.f;
        if (Button(Id("wifi.loc"), { body.cx() - 320.f, by, 310.f, 62.f }, L"Location settings", BTN_ACCENT, 0xE713))
            Defer([] { OpenUri(L"ms-settings:privacy-location"); });
        if (Button(Id("wifi.winlist"), { body.cx() + 10.f, by, 310.f, 62.f }, L"Windows Wi-Fi list", BTN_GLASS, 0xE701))
            Defer([] { OpenUri(L"ms-availablenetworks:"); });
    } else if (!n.netsValid || n.nets.empty()) {
        Message(body, 0xE701, n.netsValid ? L"No networks found" : L"Looking for networks\x2026", L"");
    } else {
        const float rowH = 84.f, expH = 96.f;
        float contentH = 0;
        for (auto& wn : n.nets) contentH += rowH + (wn.ssidRaw == g.wifi.selected ? expH : 0.f) + 10.f;
        ui.Scroll(Id("wifi.scroll"), body, &g.wifi.scroll, contentH);
        gfx.PushClip(body);
        ui.PushClip(body);
        float y = body.y - g.wifi.scroll;
        for (auto& wn : n.nets) {
            bool sel = wn.ssidRaw == g.wifi.selected;
            float h = rowH + (sel ? expH : 0.f);
            Box row{ body.x, y, body.w, h };
            if (row.b() > body.y && row.y < body.b()) WifiRow(wn, row, sel, rowH);
            y += h + 10.f;
        }
        ui.PopClip();
        gfx.PopClip();
        if (contentH > body.h) {
            float th = std::max(40.f, body.h * body.h / contentH);
            float ty = body.y + (body.h - th) * (g.wifi.scroll / (contentH - body.h));
            gfx.FillRect({ body.r() + 12.f, ty, 5.f, th }, g.th.text3, 2.5f);
        }
    }
    if (!g.wifi.status.empty()) {
        D2D1_COLOR_F c = g.wifi.statusKind == 1 ? g.th.success : g.wifi.statusKind == 2 ? g.th.danger : g.th.text2;
        gfx.Text(g.wifi.status, { x, footer.y - 52.f, w, 40.f }, FACE_TEXT, 21.f, W_SEMIBOLD, c, AL_LEFT);
    }
    if (Button(Id("wifi.settings"), { x, footer.y, 330.f, 60.f }, L"Windows Wi-Fi settings", BTN_GLASS, 0xE713))
        Defer([] { OpenUri(L"ms-settings:network-wifi"); });
    if (Button(Id("wifi.done"), { footer.r() - 170.f, footer.y, 170.f, 60.f }, L"Done", BTN_ACCENT)) Defer(CloseSheet);
    EndSheet();
}

void OnWifiEvent(const WifiEvent& e) {
    switch (e.code) {
    case WifiEvent::Info:
        g.wifi.status = e.text;
        g.wifi.statusKind = 0;
        break;
    case WifiEvent::Connected:
        g.wifi.status = Format(Tr(L"Connected to %s"), e.text.c_str());
        g.wifi.statusKind = 1;
        g.wifi.selected.clear();
        g.wifi.password.clear();
        if (g.sheet == Sheet::Wifi) edits.Clear();
        ShowToast(Format(Tr(L"Connected to %s"), e.text.c_str()), 1);
        break;
    case WifiEvent::Failed:
    case WifiEvent::ActionFailed:
        g.wifi.status = e.text;
        g.wifi.statusKind = 2;
        if (g.sheet != Sheet::Wifi || e.code == WifiEvent::Failed) ShowToast(e.text, 2);
        break;
    case WifiEvent::Disconnected:
        if (g.sheet == Sheet::Wifi && g.wifi.statusKind != 2) { g.wifi.status.clear(); }
        break;
    }
    RequestFrame();
}

// ---------------------------------------------------------------- network details
static void InfoRow(Box col, float& y, const std::wstring& k, const std::wstring& v) {
    gfx.Text(k, { col.x, y, 170.f, 40.f }, FACE_TEXT, 20.f, W_REGULAR, g.th.text2);
    gfx.Text(v.empty() ? L"\x2014" : v, { col.x + 170.f, y, col.w - 170.f, 40.f }, FACE_TEXT, 20.f, W_SEMIBOLD, g.th.text, AL_RIGHT);
    y += 44.f;
    gfx.Line(col.x, y - 3.f, col.r(), y - 3.f, MulA(g.th.text3, 0.25f), 1.f);
}

static void SheetNetwork() {
    NetInfo& n = g.net;
    std::wstring sub = n.internet ? L"Internet is reachable" : (n.lanConnected || n.wifiConnected) ? L"Connected, but no internet" : L"Offline";
    Box card = BeginSheet(1180.f, 740.f, L"Network", sub);
    float colW = (card.w - 88.f - 30.f - 2 * 20.f) / 2;
    Box lan{ card.x + 44.f + 20.f, card.y + 158.f, colW, 420.f };
    Box wif{ lan.r() + 20.f + 30.f + 20.f, lan.y, colW, 420.f };
    Glass(lan.inset(-20.f, -18.f), 26.f, 0, false);
    Glass(wif.inset(-20.f, -18.f), 26.f, 0, false);

    gfx.Glyph(0xE839, { lan.x, lan.y, 44.f, 44.f }, 26.f, g.th.text);
    gfx.Text(L"Wired (LAN)", { lan.x + 54.f, lan.y, lan.w - 54.f, 44.f }, FACE_TEXT, 26.f, W_SEMIBOLD, g.th.text);
    float y = lan.y + 62.f;
    InfoRow(lan, y, L"Status", !n.lanPresent ? L"No LAN adapter" : n.lanConnected ? L"Connected" : L"Cable unplugged");
    InfoRow(lan, y, L"Speed", n.lanConnected ? SpeedText(n.lanSpeed) : L"");
    InfoRow(lan, y, L"IP address", n.lanIp);
    InfoRow(lan, y, L"Gateway", n.lanGateway);
    InfoRow(lan, y, L"DNS", n.lanDns);
    InfoRow(lan, y, L"MAC", n.lanMac);
    InfoRow(lan, y, L"Adapter", n.lanAdapter);

    DrawWifiIcon(wif.x + 22.f, wif.y + 20.f, 30.f, n.wifiConnected ? Level(n.wifiQuality) : 0, g.th.text, MulA(g.th.text3, 0.55f));
    gfx.Text(L"Wi-Fi", { wif.x + 54.f, wif.y, wif.w - 54.f, 44.f }, FACE_TEXT, 26.f, W_SEMIBOLD, g.th.text);
    y = wif.y + 62.f;
    InfoRow(wif, y, L"Status", !n.wifiPresent ? L"No Wi-Fi adapter" : !n.wifiRadioOn ? L"Off" : n.wifiConnected ? L"Connected" : L"Not connected");
    InfoRow(wif, y, L"Network", n.wifiConnected ? n.wifiSsid : L"");
    InfoRow(wif, y, L"Signal", n.wifiConnected ? Format(L"%d%%", n.wifiQuality) : L"");
    InfoRow(wif, y, L"IP address", n.wifiIp);
    InfoRow(wif, y, L"Gateway", n.wifiGateway);
    InfoRow(wif, y, L"DNS", n.wifiDns);
    InfoRow(wif, y, L"MAC", n.wifiMac);

    float fy = card.b() - 40.f - 60.f;
    if (Button(Id("net.settings"), { card.x + 44.f, fy, 300.f, 60.f }, L"Network settings", BTN_GLASS, 0xE713))
        Defer([] { OpenUri(L"ms-settings:network"); });
    if (Button(Id("netsheet.wifi"), { card.x + 44.f + 316.f, fy, 220.f, 60.f }, L"Wi-Fi\x2026", BTN_GLASS, 0xE701))
        Defer([] { OpenSheet(Sheet::Wifi); });
    if (Button(Id("net.done"), { card.r() - 44.f - 170.f, fy, 170.f, 60.f }, L"Done", BTN_ACCENT)) Defer(CloseSheet);
    EndSheet();
}

// ---------------------------------------------------------------- settings
static Box Row(float x, float& y, float w, const std::wstring& label, const std::wstring& hint, float ctlW) {
    const float h = 72.f;
    if (hint.empty()) gfx.Text(label, { x, y, w - ctlW - 16.f, h }, FACE_TEXT, 23.f, W_SEMIBOLD, g.th.text);
    else {
        gfx.Text(label, { x, y + 6.f, w - ctlW - 16.f, 34.f }, FACE_TEXT, 23.f, W_SEMIBOLD, g.th.text);
        gfx.Text(hint, { x, y + 40.f, w - ctlW - 16.f, 26.f }, FACE_TEXT, 17.f, W_REGULAR, g.th.text3);
    }
    Box ctl{ x + w - ctlW, y + 8.f, ctlW, h - 16.f };
    y += h + 8.f;
    return ctl;
}

static void WallpaperRow(float x, float& y, float w, bool light) {
    std::wstring& path = light ? g.cfg.wallpaperLight : g.cfg.wallpaperDark;
    Box ctl = Row(x, y, w, light ? L"Light wallpaper" : L"Dark wallpaper",
                  path.empty() ? L"Built-in aurora" : path.substr(path.find_last_of(L"\\/") + 1), 340.f);
    Box thumb{ ctl.x, ctl.y - 2.f, 104.f, 60.f };
    gfx.FillRectBrush(thumb, gfx.LinearV(thumb, light ? Rgb(0xDCE6F8) : Rgb(0x1A2450), light ? Rgb(0xF6E3D8) : Rgb(0x06070D)), 12.f);
    if (!path.empty()) {
        if (ID2D1Bitmap* wb = WallpaperBitmap(light)) {
            D2D1_SIZE_F sz = wb->GetSize();
            float k = std::max(thumb.w / sz.width, thumb.h / sz.height);
            D2D1_RECT_F src = D2D1::RectF((sz.width - thumb.w / k) / 2, (sz.height - thumb.h / k) / 2,
                                          (sz.width + thumb.w / k) / 2, (sz.height + thumb.h / k) / 2);
            gfx.rt->DrawBitmap(wb, thumb.rect(), 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &src);
        } else {
            gfx.Glyph(0xE91B, thumb, 24.f, Rgb(0xFFFFFF, 0.8f));
        }
    } else {
        gfx.FillCircle(thumb.x + 28.f, thumb.y + 20.f, 22.f, light ? Rgb(0x7FB3FF, 0.6f) : Rgb(0x3558FF, 0.6f));
        gfx.FillCircle(thumb.r() - 24.f, thumb.y + 18.f, 18.f, light ? Rgb(0xC9A6FF, 0.6f) : Rgb(0x9B3BFF, 0.5f));
    }
    gfx.StrokeRect(thumb.inset(0.5f), g.th.glassStroke, 12.f, 1.f);
    uint64_t cid = light ? Id("set.wpl") : Id("set.wpd");
    if (Button(cid, { thumb.r() + 14.f, ctl.y, 140.f, 56.f }, L"Choose\x2026", BTN_GLASS)) {
        Defer([light] {
            std::wstring p;
            if (PickFile(g.hwnd, true, p)) {
                (light ? g.cfg.wallpaperLight : g.cfg.wallpaperDark) = p;
                SaveConfig();
                ShowToast(light ? L"Light wallpaper set" : L"Dark wallpaper set", 1);
            }
        });
    }
    if (!path.empty() && IconButton(cid ^ 0x5, { ctl.r() - 56.f, ctl.y, 56.f, 56.f }, 0xE711, BTN_PLAIN, 18.f)) {
        Defer([light] {
            (light ? g.cfg.wallpaperLight : g.cfg.wallpaperDark).clear();
            (light ? g.wallLight : g.wallDark) = WallEntry();
            SaveConfig();
        });
    }
}

static void SheetSettings() {
    Box card = BeginSheet(1240.f, 900.f, L"Settings", Format(L"RoomHome %s (%s)", APP_VERSION, APP_ARCH));
    float colW = (card.w - 88.f - 70.f) / 2;
    float lx = card.x + 44.f, rx = lx + colW + 70.f;
    float y = card.y + 128.f;

    Field(Id("set.title"), Row(lx, y, colW, L"Room name", L"Title at the top-left", 320.f), &g.cfg.roomName, Tr(L"Meeting Room"));
    Field(Id("set.sub"), Row(lx, y, colW, L"Subtitle", L"Blank shows the IP", 320.f), &g.cfg.subtitle, L"IP address");
    int cm = Segmented(Id("set.clock"), Row(lx, y, colW, L"Clock", L"", 320.f), { L"Auto", L"24-hour", L"12-hour" }, g.cfg.clockMode);
    if (cm != g.cfg.clockMode) { g.cfg.clockMode = cm; SaveConfig(); }
    int wsel = Segmented(Id("set.week"), Row(lx, y, colW, L"Week starts", L"", 320.f), { L"Auto", L"Sunday", L"Monday" }, g.cfg.weekStart);
    if (wsel != g.cfg.weekStart) { g.cfg.weekStart = wsel; SaveConfig(); }
    bool autoOn = g.autostartUser || g.autostartMachine;
    if (Toggle(Id("set.auto"), Row(lx, y, colW, L"Start with Windows", autoOn ? L"On: opens after sign-in" : L"Off", 120.f), autoOn)) {
        Defer([autoOn] {
            if (autoOn) {
                SetAutostartUser(false);
                if (AutostartMachine() && !SetAutostartMachine(false) && !RunSelfElevated(L"--machine-autostart off"))
                    ShowToast(L"The all-users start-up entry needs administrator approval to remove.", 2);
            } else if (!SetAutostartUser(true)) {
                ShowToast(L"Couldn't add RoomHome to start-up.", 2);
            }
            RefreshAutostart();
            bool on = g.autostartUser || g.autostartMachine;
            ShowToast(on ? L"RoomHome will start with Windows" : L"RoomHome won't start with Windows", on ? 1 : 0);
        });
    }
    if (Toggle(Id("set.awake"), Row(lx, y, colW, L"Keep screen on", L"Stop the display from sleeping", 120.f), g.cfg.keepAwake))
        Defer([] { g.cfg.keepAwake = !g.cfg.keepAwake; ApplyKeepAwake(); SaveConfig(); });
    static const int idleOpts[] = { 0, 5, 15, 30 };
    int isel = 0;
    for (int i = 0; i < 4; i++) if (g.cfg.idleMinutes == idleOpts[i]) isel = i;
    int ni = Segmented(Id("set.idle"), Row(lx, y, colW, L"Ambient clock", L"Dim to a clock when idle", 320.f),
                       { L"Off", L"5 min", L"15 min", L"30 min" }, isel);
    if (ni != isel) { g.cfg.idleMinutes = idleOpts[ni]; SaveConfig(); }
    Field(Id("set.pin"), Row(lx, y, colW, L"Admin PIN", L"Locks settings and dock editing", 200.f), &g.pinDraft, L"None",
          EF_PASSWORD | EF_NUMBER);

    y = card.y + 128.f;
    // the hint is bilingual on purpose, so the row can be found whichever language is showing
    int lg = Segmented(Id("set.lang"), Row(rx, y, colW, L"Language", L"Language \x00B7 ภาษา", 340.f),
                       { L"Auto", L"English", L"ไทย" }, g.cfg.language);
    if (lg != g.cfg.language) Defer([lg] { g.cfg.language = lg; SaveConfig(); ApplyLanguage(); ShowToast(L"Language changed", 1); });
    int th = Segmented(Id("set.theme"), Row(rx, y, colW, L"Appearance", L"Auto follows Windows", 340.f), { L"Auto", L"Dark", L"Light" }, g.cfg.theme);
    if (th != g.cfg.theme) SetTheme(th);
    Box ac = Row(rx, y, colW, L"Accent", L"", 8 * 46.f);
    for (int i = 0; i < kAccentCount; i++) {
        Box sw{ ac.x + i * 46.f, ac.y, 46.f, ac.h };
        if (Swatch(Id("set.acc", (uint64_t)i), sw, Rgb(kAccents[i]), g.cfg.accent == kAccents[i])) {
            uint32_t c = kAccents[i];
            Defer([c] { g.cfg.accent = c; SaveConfig(); });
        }
    }
    WallpaperRow(rx, y, colW, false);
    WallpaperRow(rx, y, colW, true);
    y += 12.f;
    if (Button(Id("set.dock"), { rx, y, (colW - 16.f) / 2, 62.f }, L"Edit home screen", BTN_GLASS, 0xE70F))
        Defer([] { CloseSheet(); g.editMode = true; });
    if (Button(Id("set.exit"), { rx + (colW + 16.f) / 2, y, (colW - 16.f) / 2, 62.f }, L"Exit RoomHome", BTN_GLASS, 0xE7E8))
        Defer([] { CloseSheet(); g.allowClose = true; if (g.hwnd) DestroyWindow(g.hwnd); });
    y += 80.f;
    gfx.Text(Format(Tr(L"Settings file: %s"), ConfigPath().c_str()), { rx, y, colW, 28.f }, FACE_TEXT, 16.f, W_REGULAR, g.th.text3);

    float fy = card.b() - 40.f - 60.f;
    if (Button(Id("set.done"), { card.r() - 44.f - 170.f, fy, 170.f, 60.f }, L"Done", BTN_ACCENT)) Defer(CloseSheet);
    EndSheet();
}

// ---------------------------------------------------------------- power
static bool HoldTile(uint64_t id, Box b, wchar_t glyph, const std::wstring& label, D2D1_COLOR_F col) {
    ui.Add(id, b);
    float p = ui.HoldProgress(id, 1.5f);
    float hov = ui.Anim(id ^ 0x51, ui.Highlighted(id) ? 1.f : 0.f, 16.f);
    float shown = ui.Anim(id ^ 0x61, p, p > 0 ? 40.f : 10.f);
    Box bb = b.scaled(1.f + 0.025f * hov - (ui.Pressed(id) ? 0.02f : 0.f));
    Glass(bb, 30.f, hov, true);
    if (shown > 0.002f) {
        gfx.PushClip({ bb.x, bb.y, bb.w * shown, bb.h });
        gfx.FillRect(bb, MulA(col, 0.30f), 30.f);
        gfx.PopClip();
    }
    float cx = bb.cx(), cy = bb.y + bb.h * 0.38f, r = 46.f;
    gfx.Shadow({ cx - r, cy - r, 2 * r, 2 * r }, r, 14.f, 0.3f, 4.f, Darken(col, 0.5f));
    gfx.FillCircle(cx, cy, r, col);
    gfx.Glyph(glyph, { cx - r, cy - r, 2 * r, 2 * r }, 36.f, Rgb(0xFFFFFF));
    gfx.StrokeCircle(cx, cy, r + 10.f, MulA(g.th.text3, 0.35f), 4.f);
    if (shown > 0.002f) gfx.Arc(cx, cy, r + 10.f, -90.f, -90.f + 360.f * shown, col, 5.f);
    gfx.Text(label, { bb.x, bb.y + bb.h * 0.66f, bb.w, 40.f }, FACE_TEXT, 28.f, W_SEMIBOLD, g.th.text, AL_CENTER);
    gfx.Text(p > 0 ? L"Keep holding\x2026" : L"Hold to confirm", { bb.x, bb.y + bb.h * 0.66f + 40.f, bb.w, 30.f }, FACE_TEXT, 19.f,
             W_REGULAR, p > 0 ? col : g.th.text3, AL_CENTER);
    FocusRing(id, bb, 30.f);
    return p >= 1.f;
}

static void DoPower(bool restart) {
    CloseSheet();
    if (g.dryRun || g.screenshot) {
        ShowToast(restart ? L"Test mode: the PC would restart now" : L"Test mode: the PC would shut down now", 1);
        return;
    }
    g.shuttingDown = true;
    g.shutdownText = restart ? L"Restarting\x2026" : L"Shutting down\x2026";
    SaveConfig();
    std::wstring err;
    if (!PowerAction(restart, err)) {
        g.shuttingDown = false;
        ShowToast(Format(Tr(L"Windows refused: %s"), err.c_str()), 2);
    }
    RequestFrame();
}

static void SheetPower() {
    Box card = BeginSheet(880.f, 560.f, L"Power", L"Press and hold for 1.5 seconds");
    float tw = (card.w - 88.f - 30.f) / 2, ty = card.y + 136.f, tht = 280.f;
    if (HoldTile(Id("pw.off"), { card.x + 44.f, ty, tw, tht }, 0xE7E8, L"Shut down", g.th.danger)) Defer([] { DoPower(false); });
    if (HoldTile(Id("pw.re"), { card.x + 44.f + tw + 30.f, ty, tw, tht }, 0xE777, L"Restart", g.th.accent)) Defer([] { DoPower(true); });
    if (Button(Id("pw.cancel"), { card.cx() - 110.f, card.b() - 40.f - 60.f, 220.f, 60.f }, L"Cancel", BTN_GLASS)) Defer(CloseSheet);
    EndSheet();
}

// ---------------------------------------------------------------- add / edit
static void AddPreset(int i) {
    const Preset& p = kPresets[i];
    DockItem it = ItemFromPreset(p);
    if (IsCustomKind(p.kind) && Trim(p.target).empty()) { OpenEditor(-1, it, true); return; }
    if ((int)g.cfg.dock.size() >= g.cfg.dockCapacity) { ShowToast(L"The dock is full. Remove an app first.", 2); return; }
    g.cfg.dock.push_back(it);
    SaveConfig();
    CloseSheet();
    ShowToast(Format(Tr(L"Added %s to the dock"), Tr(it.name).c_str()), 1);
}

static void SheetAdd() {
    int n = (int)g.cfg.dock.size();
    Box card = BeginSheet(1160.f, 840.f, L"Add to the dock", Format(Tr(L"%d of %d places used"), n, g.cfg.dockCapacity));
    const int cols = 4;
    float gap = 22.f;
    float cw = (card.w - 88.f - (cols - 1) * gap) / cols, chh = 168.f;
    for (int i = 0; i < kPresetCount; i++) {
        Box cell{ card.x + 44.f + (i % cols) * (cw + gap), card.y + 132.f + (i / cols) * (chh + gap), cw, chh };
        uint64_t id = Id("add.p", (uint64_t)i);
        ui.Add(id, cell);
        float hov = ui.Anim(id ^ 0x51, ui.Highlighted(id) ? 1.f : 0.f, 16.f);
        float prs = ui.Anim(id ^ 0x52, ui.Pressed(id) ? 1.f : 0.f, 24.f);
        Box cb = cell.scaled(1.f + 0.03f * hov - 0.04f * prs);
        gfx.FillRect(cb, Mix(g.th.glass, g.th.glassHi, hov), 24.f);
        DockItem tmp = ItemFromPreset(kPresets[i]);
        tmp.uid = 0;
        float tw = cb.w - 64.f, th = std::round(tw * 0.5f);
        DrawTileFace(tmp, { cb.cx() - tw / 2, cb.y + 18.f, tw, th }, hov * 0.6f, cb.cx(), cb.y + 18.f);
        gfx.Text(kPresets[i].name, { cb.x + 8.f, cb.y + 26.f + th, cb.w - 16.f, 40.f }, FACE_TEXT, 21.f, W_SEMIBOLD, g.th.text, AL_CENTER);
        FocusRing(id, cb, 24.f);
        if (ui.Clicked(id)) Defer([i] { AddPreset(i); });
    }
    if (Button(Id("add.cancel"), { card.r() - 44.f - 170.f, card.b() - 40.f - 60.f, 170.f, 60.f }, L"Cancel", BTN_GLASS)) Defer(CloseSheet);
    EndSheet();
}

void OpenEditor(int index, const DockItem& item, bool isNew) {
    g.edit.item = item;
    // a new app's name is the user's to edit: start it in the language on screen, so the
    // name box and the tile preview agree ("Program" -> "โปรแกรม")
    if (isNew) g.edit.item.name = Tr(item.name);
    g.edit.index = index;
    g.edit.isNew = isNew;
    OpenSheet(Sheet::EditApp);
}

void RemoveDockItem(int index) {
    if (index < 0 || index >= (int)g.cfg.dock.size()) return;
    DockItem it = g.cfg.dock[index];
    g.cfg.dock.erase(g.cfg.dock.begin() + index);
    SaveConfig();
    ShowToast(Format(Tr(L"Removed %s"), Tr(it.name).c_str()), 0, L"Undo", [it, index] {
        if ((int)g.cfg.dock.size() >= g.cfg.dockCapacity) return;
        int at = std::min(index, (int)g.cfg.dock.size());
        g.cfg.dock.insert(g.cfg.dock.begin() + at, it);
        SaveConfig();
    });
}

static void SaveEditor() {
    DockItem it = g.edit.item;
    it.name = Trim(it.name);
    it.target = Trim(it.target);
    it.args = Trim(it.args);
    if (it.name.empty()) { ShowToast(L"Give the app a name.", 2); return; }
    if (it.kind == L"web" && it.target.empty()) { ShowToast(L"Type a web address.", 2); return; }
    if (it.kind == L"program" && it.target.empty()) { ShowToast(L"Choose a program, shortcut or file.", 2); return; }
    if (g.edit.isNew) {
        if ((int)g.cfg.dock.size() >= g.cfg.dockCapacity) { ShowToast(L"The dock is full. Remove an app first.", 2); return; }
        g.cfg.dock.push_back(it);
    } else if (g.edit.index >= 0 && g.edit.index < (int)g.cfg.dock.size()) {
        g.cfg.dock[g.edit.index] = it;
    }
    InvalidateIcon(it.uid);
    SaveConfig();
    bool isNew = g.edit.isNew;
    CloseSheet();
    ShowToast(Format(Tr(isNew ? L"Added %s" : L"Saved %s"), Tr(it.name).c_str()), 1);
}

static void MoveEdited(int dir) {
    int i = g.edit.index, j = i + dir;
    auto& d = g.cfg.dock;
    if (i < 0 || j < 0 || i >= (int)d.size() || j >= (int)d.size()) return;
    std::swap(d[i], d[j]);
    g.edit.index = j;
    SaveConfig();
}

static void SheetEdit() {
    EditUi& e = g.edit;
    DockItem& it = e.item;
    const Preset* pre = PresetForKind(it.kind);
    std::wstring sub = it.kind == L"web" ? L"Website \x00B7 opens in Microsoft Edge"
                     : it.kind == L"program" ? L"Program, shortcut or document"
                     : pre ? Format(Tr(L"%s launcher"), Tr(pre->name)) : it.kind;
    Box card = BeginSheet(1160.f, 880.f, e.isNew ? L"New app" : L"Edit app", sub);
    float lx = card.x + 44.f, lw = 330.f;
    float rx = lx + lw + 56.f, rw = card.r() - 44.f - rx;

    Box pv{ lx, card.y + 140.f, lw, std::round(lw * 0.6f) };
    gfx.Shadow(pv, pv.h * 0.16f, 22.f, Lerp(0.34f, 0.14f, g.th.light), 10.f, g.th.shadow);
    DrawTileFace(it, pv, 0.35f, pv.x + pv.w * 0.3f, pv.y);
    gfx.Text(it.name.empty() ? L"App name" : it.name, { lx - 10.f, pv.b() + 14.f, lw + 20.f, 40.f }, FACE_TEXT, 24.f, W_SEMIBOLD,
             it.name.empty() ? g.th.text3 : g.th.text, AL_CENTER);
    std::wstring note = pre ? pre->note : it.kind == L"web" ? L"Any web address, opened in a new Microsoft Edge window."
                      : L"Pick an .exe, a shortcut (.lnk) or a document. Store apps work too: shell:AppsFolder\\<app id>.";
    gfx.Text(note, { lx, pv.b() + 70.f, lw, 200.f }, FACE_TEXT, 19.f, W_REGULAR, g.th.text2, AL_LEFT, false, true);

    float y = card.y + 132.f;
    Label(L"Name", rx, y, rw);
    Field(Id("ed.name"), { rx, y + 32.f, rw, 62.f }, &it.name, L"App name", e.isNew ? EF_AUTOFOCUS : 0);
    y += 112.f;
    if (it.kind == L"web") {
        Label(L"Web address", rx, y, rw);
        Field(Id("ed.url"), { rx, y + 32.f, rw, 62.f }, &it.target, L"https://");
        y += 112.f;
    } else if (it.kind == L"program") {
        Label(L"Program, shortcut or file", rx, y, rw);
        Field(Id("ed.path"), { rx, y + 32.f, rw - 170.f, 62.f }, &it.target, L"C:\\Program Files\\App\\app.exe");
        if (Button(Id("ed.browse"), { rx + rw - 156.f, y + 32.f, 156.f, 62.f }, L"Browse\x2026", BTN_GLASS)) {
            Defer([] {
                std::wstring p;
                if (!PickFile(g.hwnd, false, p)) return;
                DockItem& d = g.edit.item;
                d.target = p;
                edits.SetText(Id("ed.path"), p);
                std::wstring base = p.substr(p.find_last_of(L"\\/") + 1);
                size_t dot = base.find_last_of(L'.');
                if (dot != std::wstring::npos) base = base.substr(0, dot);
                if (Trim(d.name).empty() || d.name == L"Program") { d.name = base; edits.SetText(Id("ed.name"), base); }
                d.glyph = L"appicon";
                InvalidateIcon(d.uid);
            });
        }
        y += 112.f;
        Label(L"Arguments (optional)", rx, y, rw);
        Field(Id("ed.args"), { rx, y + 32.f, rw, 62.f }, &it.args, L"");
        y += 112.f;
    }

    Label(L"Icon", rx, y, rw);
    const int perRow = 12;
    float cs = std::min(54.f, rw / perRow);
    int total = kGlyphCount + 1;
    for (int k = 0; k < total; k++) {
        bool isApp = k == kGlyphCount;
        Box cell{ rx + (k % perRow) * cs, y + 34.f + (k / perRow) * cs, cs - 6.f, cs - 6.f };
        uint64_t id = Id("ed.g", (uint64_t)k);
        bool selected = isApp ? it.glyph == L"appicon" : it.glyph == kGlyphs[k].key;
        ui.Add(id, cell);
        float hov = ui.Anim(id ^ 0x51, ui.Highlighted(id) ? 1.f : 0.f, 16.f);
        gfx.FillRect(cell, selected ? g.th.accent : MulA(g.th.glassHi, 0.35f + 0.5f * hov), 12.f);
        if (isApp) gfx.Text(L"App", cell, FACE_TEXT, 15.f, W_BOLD, selected ? g.th.onAccent : g.th.text, AL_CENTER);
        else gfx.Glyph(kGlyphs[k].ch, cell, 22.f, selected ? g.th.onAccent : g.th.text);
        FocusRing(id, cell, 12.f);
        if (ui.Clicked(id)) {
            std::wstring key = isApp ? L"appicon" : kGlyphs[k].key;
            uint32_t uid = it.uid;
            Defer([key, uid] { g.edit.item.glyph = key; InvalidateIcon(uid); });
        }
    }
    y += 34.f + ((total + perRow - 1) / perRow) * cs + 14.f;

    Label(L"Colour", rx, y, rw);
    for (int k = 0; k < kSwatchCount; k++) {
        Box sw{ rx + k * 54.f, y + 30.f, 54.f, 54.f };
        if (Swatch(Id("ed.c", (uint64_t)k), sw, Rgb(kSwatches[k]), it.color == kSwatches[k])) {
            uint32_t c = kSwatches[k];
            Defer([c] { g.edit.item.color = c; });
        }
    }

    float fy = card.b() - 40.f - 60.f;
    float fx = card.x + 44.f;
    if (!e.isNew) {
        if (Button(Id("ed.remove"), { fx, fy, 170.f, 60.f }, L"Remove", BTN_DANGER, 0xE74D)) {
            int idx = e.index;
            Defer([idx] { CloseSheet(); RemoveDockItem(idx); });
        }
        fx += 186.f;
        if (IconButton(Id("ed.left"), { fx, fy, 60.f, 60.f }, 0xE76B, BTN_GLASS, 20.f, e.index > 0)) Defer([] { MoveEdited(-1); });
        fx += 72.f;
        if (IconButton(Id("ed.right"), { fx, fy, 60.f, 60.f }, 0xE76C, BTN_GLASS, 20.f, e.index + 1 < (int)g.cfg.dock.size()))
            Defer([] { MoveEdited(1); });
    }
    if (Button(Id("ed.save"), { card.r() - 44.f - 190.f, fy, 190.f, 60.f }, e.isNew ? L"Add to dock" : L"Save", BTN_ACCENT))
        Defer(SaveEditor);
    if (Button(Id("ed.cancel"), { card.r() - 44.f - 190.f - 16.f - 160.f, fy, 160.f, 60.f }, L"Cancel", BTN_GLASS)) Defer(CloseSheet);
    EndSheet();
}

// ---------------------------------------------------------------- widgets
static const WidgetType* TypeInfo(const std::wstring& t) { return WidgetTypeOf(t); }

static void AddWidgetOfType(const std::wstring& type) {
    if ((int)g.cfg.widgets.size() >= kMaxWidgets) {
        ShowToast(L"There is room for 4 widgets. Remove one first.", 2);
        return;
    }
    g.cfg.widgets.push_back(NewWidget(type));
    SaveConfig();
    int idx = (int)g.cfg.widgets.size() - 1;
    if (type == L"note" || type == L"worldclock" || type == L"photo") { OpenWidgetEditor(idx); return; }
    CloseSheet();
    const WidgetType* t = TypeInfo(type);
    ShowToast(Format(Tr(L"Added %s"), Tr(t ? t->name : L"Widget")), 1);
}

static void SheetAddWidget() {
    int n = (int)g.cfg.widgets.size();
    Box card = BeginSheet(1160.f, 780.f, L"Add a widget", Format(Tr(L"%d of %d places used"), n, kMaxWidgets));
    static std::vector<WidgetCfg> samples;
    if (samples.empty())
        for (int i = 0; i < kWidgetTypeCount; i++) { WidgetCfg w = NewWidget(kWidgetTypes[i].type); w.uid = 0; samples.push_back(w); }
    const int cols = 3;
    float gap = 24.f;
    float cw = (card.w - 88.f - (cols - 1) * gap) / cols, chh = 230.f;
    for (int i = 0; i < kWidgetTypeCount; i++) {
        Box cell{ card.x + 44.f + (i % cols) * (cw + gap), card.y + 132.f + (i / cols) * (chh + gap), cw, chh };
        uint64_t id = Id("aw.t", (uint64_t)i);
        ui.Add(id, cell);
        float hov = ui.Anim(id ^ 0x51, ui.Highlighted(id) ? 1.f : 0.f, 16.f);
        float prs = ui.Anim(id ^ 0x52, ui.Pressed(id) ? 1.f : 0.f, 24.f);
        Box cb = cell.scaled(1.f + 0.03f * hov - 0.04f * prs);
        gfx.FillRect(cb, Mix(g.th.glass, g.th.glassHi, hov), 24.f);
        DrawWidgetPreview(samples[i], { cb.x + 16.f, cb.y + 14.f, cb.w - 32.f, 150.f });
        gfx.Text(kWidgetTypes[i].name, { cb.x + 8.f, cb.y + 172.f, cb.w - 16.f, 40.f }, FACE_TEXT, 22.f, W_SEMIBOLD, g.th.text, AL_CENTER);
        FocusRing(id, cb, 24.f);
        if (ui.Clicked(id)) {
            std::wstring type = kWidgetTypes[i].type;
            Defer([type] { AddWidgetOfType(type); });
        }
    }
    if (Button(Id("aw.cancel"), { card.r() - 44.f - 170.f, card.b() - 40.f - 60.f, 170.f, 60.f }, L"Cancel", BTN_GLASS)) Defer(CloseSheet);
    EndSheet();
}

// City name for a zone: the city the search matched, else the first one Windows lists.
static std::wstring AutoCity(const ZoneInfo& z, const std::wstring& filter) {
    std::wstring d = z.display;
    size_t p = d.find(L") ");
    if (!d.empty() && d[0] == L'(' && p != std::wstring::npos) d = d.substr(p + 2);
    std::vector<std::wstring> parts;
    size_t s = 0;
    while (s <= d.size()) {
        size_t e = d.find(L", ", s);
        parts.push_back(Trim(d.substr(s, e == std::wstring::npos ? std::wstring::npos : e - s)));
        if (e == std::wstring::npos) break;
        s = e + 2;
    }
    std::wstring f = ToLower(Trim(filter));
    if (!f.empty())
        for (auto& part : parts) if (ToLower(part).find(f) != std::wstring::npos) return part;
    if (d == L"Coordinated Universal Time") return L"UTC";
    return parts.empty() ? d : parts[0];
}

static std::vector<const ZoneInfo*> MatchingZones(const std::wstring& filter) {
    std::vector<const ZoneInfo*> out;
    std::wstring f = ToLower(Trim(filter));
    for (auto& z : TimeZones()) {
        if (!f.empty()) {
            int h = std::abs(z.offsetMin) / 60, m = std::abs(z.offsetMin) % 60;
            std::wstring hay = ToLower(z.display + L" " + z.key) +
                               Format(m ? L" utc%c%d:%02d" : L" utc%c%d", z.offsetMin < 0 ? L'-' : L'+', h, m);
            if (hay.find(f) == std::wstring::npos) continue;
        }
        out.push_back(&z);
    }
    return out;
}

static void PickZone(const ZoneInfo& z) {
    WidgetEditUi& e = g.wedit;
    std::wstring city = AutoCity(z, e.tzFilter);
    if (Trim(e.w.city).empty() || e.w.city == e.lastAutoCity) {
        e.w.city = city;
        e.lastAutoCity = city;
        edits.SetText(Id("we.city"), city);
    }
    e.w.zone = z.key;
}

void OpenWidgetEditor(int index) {
    if (index < 0 || index >= (int)g.cfg.widgets.size()) return;
    g.wedit = WidgetEditUi();
    g.wedit.w = g.cfg.widgets[index];
    g.wedit.index = index;
    g.wedit.lastAutoCity = g.wedit.w.city;   // a zone picked now may rename it until the user types a name
    if (g.wedit.w.type == L"worldclock") {
        auto all = MatchingZones(L"");
        for (size_t i = 0; i < all.size(); i++)
            if (all[i]->key == g.wedit.w.zone) g.wedit.tzScroll = std::max(0.f, (float)i * 52.f - 104.f);
    }
    OpenSheet(Sheet::EditWidget);
}

void RemoveWidget(int index) {
    if (index < 0 || index >= (int)g.cfg.widgets.size()) return;
    WidgetCfg w = g.cfg.widgets[index];
    g.cfg.widgets.erase(g.cfg.widgets.begin() + index);
    SaveConfig();
    const WidgetType* t = TypeInfo(w.type);
    ShowToast(Format(Tr(L"Removed %s"), Tr(t ? t->name : L"Widget")), 0, L"Undo", [w, index] {
        if ((int)g.cfg.widgets.size() >= kMaxWidgets) return;
        int at = std::min(index, (int)g.cfg.widgets.size());
        g.cfg.widgets.insert(g.cfg.widgets.begin() + at, w);
        SaveConfig();
    });
}

static void SaveWidgetEditor() {
    WidgetCfg w = g.wedit.w;
    w.city = Trim(w.city);
    w.title = Trim(w.title);
    if (g.wedit.index >= 0 && g.wedit.index < (int)g.cfg.widgets.size()) g.cfg.widgets[g.wedit.index] = w;
    SaveConfig();
    CloseSheet();
    const WidgetType* t = TypeInfo(w.type);
    ShowToast(Format(Tr(L"Saved %s"), Tr(t ? t->name : L"Widget")), 1);
}

static void MoveWidget(int dir) {
    int i = g.wedit.index, j = i + dir;
    auto& v = g.cfg.widgets;
    if (i < 0 || j < 0 || i >= (int)v.size() || j >= (int)v.size()) return;
    std::swap(v[i], v[j]);
    g.wedit.index = j;
    SaveConfig();
}

static void SheetEditWidget() {
    WidgetEditUi& e = g.wedit;
    WidgetCfg& w = e.w;
    const WidgetType* wt = TypeInfo(w.type);
    Box card = BeginSheet(1180.f, 900.f, wt ? wt->name : L"Widget", L"Changes show in the preview. Save puts them on the home screen.");
    float lx = card.x + 44.f, lw = 400.f;
    float rx = lx + lw + 56.f, rw = card.r() - 44.f - rx;

    Box stage{ lx, card.y + 136.f, lw, 340.f };
    gfx.FillRect(stage, MulA(g.th.track, 0.55f), 26.f);
    DrawWidgetPreview(w, stage.inset(18.f));
    if (wt) gfx.Text(wt->desc, { lx, stage.b() + 22.f, lw, 120.f }, FACE_TEXT, 19.f, W_REGULAR, g.th.text2, AL_LEFT, false, true);

    float y = card.y + 128.f;
    auto toggleRow = [&](const char* key, const wchar_t* label, const wchar_t* hint, bool& v) {
        if (Toggle(Id(key), Row(rx, y, rw, label, hint, 120.f), v)) v = !v;
    };
    if (w.type == L"analog") {
        toggleRow("we.sec", L"Second hand", L"Ticks once a second", w.seconds);
        toggleRow("we.num", L"Numbers", L"1 to 12 around the dial", w.numbers);
        toggleRow("we.date", L"Date", L"Weekday and day of the month", w.date);
    } else if (w.type == L"digital") {
        toggleRow("we.sec", L"Seconds", L"Shown next to the minutes", w.seconds);
        toggleRow("we.greet", L"Greeting", L"Good morning / afternoon / evening", w.greeting);
        toggleRow("we.date", L"Date", L"Full date under the time", w.date);
        int sz = Segmented(Id("we.size"), Row(rx, y, rw, L"Size", L"", 300.f), { L"Normal", L"Large" }, w.large ? 1 : 0);
        w.large = sz == 1;
        gfx.Text(L"12- or 24-hour time: Settings \x203A Clock.", { rx, y + 6.f, rw, 30.f }, FACE_TEXT, 18.f, W_REGULAR, g.th.text3);
    } else if (w.type == L"calendar") {
        toggleRow("we.wk", L"Week numbers", L"ISO week numbers on the left", w.weekNumbers);
        gfx.Text(L"First day of the week: Settings \x203A Week starts.", { rx, y + 6.f, rw, 30.f }, FACE_TEXT, 18.f, W_REGULAR, g.th.text3);
    } else if (w.type == L"worldclock") {
        Label(L"City name", rx, y, rw);
        Field(Id("we.city"), { rx, y + 32.f, rw, 60.f }, &w.city, L"For example: Tokyo");
        y += 106.f;
        int st = Segmented(Id("we.style"), Row(rx, y, rw, L"Clock", L"", 300.f), { L"Digital", L"Analog" }, w.analog ? 1 : 0);
        w.analog = st == 1;
        toggleRow("we.sec", L"Seconds", L"", w.seconds);
        Label(L"Time zone", rx, y, rw);
        Field(Id("we.tzf"), { rx, y + 32.f, rw, 58.f }, &e.tzFilter, L"Search: a city, or UTC+7");
        y += 104.f;
        Box list{ rx, y, rw, card.b() - 40.f - 60.f - 24.f - y };
        gfx.FillRect(list, MulA(g.th.track, 0.55f), 18.f);
        auto matches = MatchingZones(e.tzFilter);
        const float rh = 52.f;
        ui.Scroll(Id("we.tzs"), list, &e.tzScroll, matches.size() * rh + 8.f);
        gfx.PushClip(list);
        ui.PushClip(list);
        for (size_t i = 0; i < matches.size(); i++) {
            Box row{ list.x + 6.f, list.y + 4.f + i * rh - e.tzScroll, list.w - 12.f, rh - 4.f };
            if (row.b() < list.y || row.y > list.b()) continue;
            const ZoneInfo* z = matches[i];
            uint64_t id = Mix64(HashBytes(z->key.data(), z->key.size() * sizeof(wchar_t), Id("we.tz")));
            ui.Add(id, row);
            bool sel = z->key == w.zone;
            float hov = ui.Anim(id ^ 0x51, ui.Highlighted(id) ? 1.f : 0.f, 16.f);
            if (sel) gfx.FillRect(row, g.th.accent, 12.f);
            else gfx.FillRect(row, MulA(g.th.glassHi, 0.7f * hov), 12.f);
            gfx.Text(z->display, row.inset(16.f, 0), FACE_TEXT, 19.f, sel ? W_SEMIBOLD : W_REGULAR, sel ? g.th.onAccent : g.th.text);
            if (ui.Clicked(id)) Defer([z] { PickZone(*z); });
        }
        if (matches.empty())
            gfx.Text(L"No time zone matches that search.", list.inset(20.f, 0), FACE_TEXT, 19.f, W_REGULAR, g.th.text3);
        ui.PopClip();
        gfx.PopClip();
    } else if (w.type == L"photo") {
        Label(L"Picture", rx, y, rw);
        float bw = (rw - 16.f) / 2;
        if (Button(Id("we.pfile"), { rx, y + 32.f, bw, 60.f }, L"Choose photo\x2026", BTN_GLASS, 0xE91B)) {
            Defer([] {
                std::wstring p;
                if (PickFile(g.hwnd, true, p)) { g.wedit.w.path = p; g.wedit.w.folder = false; }
            });
        }
        if (Button(Id("we.pdir"), { rx + bw + 16.f, y + 32.f, bw, 60.f }, L"Choose folder\x2026", BTN_GLASS, 0xE8B7)) {
            Defer([] {
                std::wstring p;
                if (PickFolder(g.hwnd, p)) { g.wedit.w.path = p; g.wedit.w.folder = true; }
            });
        }
        std::wstring src = L"No picture chosen yet";
        if (!Trim(w.path).empty()) {
            std::wstring leaf = w.path.substr(w.path.find_last_of(L"\\/") + 1);
            if (w.folder) {
                size_t count = 0;
                WIN32_FIND_DATAW fd;
                HANDLE h = FindFirstFileExW((w.path + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0);
                if (h != INVALID_HANDLE_VALUE) {
                    do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsImageFile(fd.cFileName)) count++; } while (FindNextFileW(h, &fd) && count < 5000);
                    FindClose(h);
                }
                src = Format(Tr(L"Slideshow of folder \x201C%s\x201D (%zu pictures)"), leaf.c_str(), count);
            } else {
                src = Format(Tr(L"Picture: %s"), leaf.c_str());
            }
        }
        gfx.Text(src, { rx, y + 100.f, rw, 30.f }, FACE_TEXT, 19.f, W_REGULAR, g.th.text2);
        y += 142.f;
        if (w.folder) {
            static const int secsOpts[] = { 10, 30, 60, 300 };
            int sel = 1;
            for (int i = 0; i < 4; i++) if (w.interval == secsOpts[i]) sel = i;
            int ns = Segmented(Id("we.int"), Row(rx, y, rw, L"Change every", L"", 380.f), { L"10 s", L"30 s", L"1 min", L"5 min" }, sel);
            w.interval = secsOpts[ns];
            toggleRow("we.shuf", L"Shuffle", L"Mix the order of the pictures", w.shuffle);
        }
        w.shape = Segmented(Id("we.shape"), Row(rx, y, rw, L"Shape", L"", 380.f), { L"Landscape", L"Square", L"Portrait" }, w.shape);
        int ft = Segmented(Id("we.fit"), Row(rx, y, rw, L"Framing", w.fit ? L"Shows the whole picture" : L"Fills the frame, trims edges", 300.f),
                           { L"Fill", L"Fit" }, w.fit ? 1 : 0);
        w.fit = ft == 1;
        Label(L"Caption (optional)", rx, y, rw);
        Field(Id("we.cap"), { rx, y + 32.f, rw, 60.f }, &w.caption, L"For example: Our team, 2026");
    } else if (w.type == L"note") {
        Label(L"Title", rx, y, rw);
        Field(Id("we.title"), { rx, y + 32.f, rw, 60.f }, &w.title, L"Title", EF_AUTOFOCUS);
        y += 106.f;
        Label(L"Message (Enter starts a new line)", rx, y, rw);
        Field(Id("we.text"), { rx, y + 32.f, rw, 200.f }, &w.text, L"Type the message", EF_MULTILINE);
        y += 248.f;
        int sz = Segmented(Id("we.size"), Row(rx, y, rw, L"Text size", L"", 300.f), { L"Normal", L"Large" }, w.large ? 1 : 0);
        w.large = sz == 1;
        Label(L"Colour", rx, y, rw);
        for (int k = 0; k < kNoteColorCount; k++) {
            Box sw{ rx + k * 56.f, y + 30.f, 56.f, 56.f };
            uint32_t c = kNoteColors[k];
            if (Swatch(Id("we.col", (uint64_t)k), sw, c ? Rgb(c) : Mix(Rgb(0x8E8E93), Rgb(0xD1D1D6), g.th.light), w.color == c)) w.color = c;
            if (!c) gfx.Line(sw.cx() - 12.f, sw.cy() + 12.f, sw.cx() + 12.f, sw.cy() - 12.f, Rgb(0xFFFFFF, 0.9f), 2.5f);
        }
    }

    float fy = card.b() - 40.f - 60.f, fx = card.x + 44.f;
    if (Button(Id("we.remove"), { fx, fy, 170.f, 60.f }, L"Remove", BTN_DANGER, 0xE74D)) {
        int idx = e.index;
        Defer([idx] { CloseSheet(); RemoveWidget(idx); });
    }
    fx += 186.f;
    if (IconButton(Id("we.left"), { fx, fy, 60.f, 60.f }, 0xE76B, BTN_GLASS, 20.f, e.index > 0)) Defer([] { MoveWidget(-1); });
    fx += 72.f;
    if (IconButton(Id("we.right"), { fx, fy, 60.f, 60.f }, 0xE76C, BTN_GLASS, 20.f, e.index + 1 < (int)g.cfg.widgets.size()))
        Defer([] { MoveWidget(1); });
    if (Button(Id("we.save"), { card.r() - 44.f - 170.f, fy, 170.f, 60.f }, L"Save", BTN_ACCENT)) Defer(SaveWidgetEditor);
    if (Button(Id("we.cancel"), { card.r() - 44.f - 170.f - 16.f - 160.f, fy, 160.f, 60.f }, L"Cancel", BTN_GLASS)) Defer(CloseSheet);
    EndSheet();
}

// ---------------------------------------------------------------- rename
static void ApplyRename() {
    std::wstring t = Trim(g.renameTitle);
    g.cfg.roomName = t;
    g.cfg.subtitle = Trim(g.renameSubtitle);
    SaveConfig();
    CloseSheet();
}

static void SheetRename() {
    Box card = BeginSheet(860.f, 500.f, L"Room name", L"Shown at the top-left of the home screen");
    float x = card.x + 44.f, w = card.w - 88.f, y = card.y + 132.f;
    Label(L"Title", x, y, w);
    Field(Id("rn.title"), { x, y + 32.f, w, 64.f }, &g.renameTitle, Tr(L"Meeting Room"), EF_AUTOFOCUS);
    y += 116.f;
    Label(L"Subtitle", x, y, w);
    Field(Id("rn.sub"), { x, y + 32.f, w, 64.f }, &g.renameSubtitle, L"Leave empty to show the IP address");
    float fy = card.b() - 40.f - 60.f;
    if (Button(Id("rn.save"), { card.r() - 44.f - 170.f, fy, 170.f, 60.f }, L"Save", BTN_ACCENT)) Defer(ApplyRename);
    if (Button(Id("rn.cancel"), { card.r() - 44.f - 170.f - 16.f - 160.f, fy, 160.f, 60.f }, L"Cancel", BTN_GLASS)) Defer(CloseSheet);
    EndSheet();
}

// ---------------------------------------------------------------- PIN
void OnPinChar(wchar_t c) {
    if (g.sheet != Sheet::Pin || !g.sheetOpen) return;
    PinUi& p = g.pin;
    if (c >= L'0' && c <= L'9') {
        if (p.entered.size() < 12) p.entered += c;
        p.wrong = false;
    } else if (c == L'\b') {
        if (!p.entered.empty()) p.entered.pop_back();
    } else if (c == 27) {
        CloseSheet();
        return;
    }
    const std::wstring& pin = g.cfg.adminPin;
    if (!pin.empty() && p.entered.size() >= pin.size()) {
        if (p.entered == pin) {
            g.unlockedUntil = g.now + 300.0;
            auto then = std::move(p.then);
            p = PinUi();
            CloseSheet();
            if (then) then();
        } else {
            p.wrong = true;
            p.shakeAt = g.now;
            p.entered.clear();
        }
    }
    RequestFrame();
}

static void SheetPin() {
    Box card = BeginSheet(560.f, 820.f, L"Admin PIN", L"Enter the PIN to continue");
    size_t n = std::max<size_t>(4, g.cfg.adminPin.size());
    float st = (float)(g.now - g.pin.shakeAt);
    float shake = st < 0.5f ? std::sin(st * 42.f) * 16.f * (1.f - st / 0.5f) : 0.f;
    if (st < 0.5f) ui.KeepAnimating("shake");
    float dw = 34.f;
    float dx = card.cx() - n * dw / 2 + shake;
    for (size_t i = 0; i < n; i++) {
        float cx = dx + (i + 0.5f) * dw, cy = card.y + 160.f;
        if (i < g.pin.entered.size()) gfx.FillCircle(cx, cy, 9.f, g.th.text);
        else gfx.StrokeCircle(cx, cy, 9.f, g.th.text3, 2.f);
    }
    if (g.pin.wrong) gfx.Text(L"Wrong PIN, try again", { card.x, card.y + 188.f, card.w, 30.f }, FACE_TEXT, 20.f, W_SEMIBOLD, g.th.danger, AL_CENTER);
    const wchar_t* keys[12] = { L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", L"", L"0", L"\x232B" };
    float ks = 112.f, kg = 22.f;
    float kx = card.cx() - (3 * ks + 2 * kg) / 2, ky = card.y + 236.f;
    for (int i = 0; i < 12; i++) {
        if (!keys[i][0]) continue;
        Box kb{ kx + (i % 3) * (ks + kg), ky + (i / 3) * (ks * 0.82f + kg), ks, ks * 0.82f };
        wchar_t ch = i == 11 ? L'\b' : keys[i][0];
        if (Button(Id("pin.k", (uint64_t)i), kb, i == 11 ? L"" : keys[i], BTN_GLASS, i == 11 ? (wchar_t)0xE750 : 0))
            Defer([ch] { OnPinChar(ch); });
    }
    if (Button(Id("pin.cancel"), { card.cx() - 110.f, card.b() - 40.f - 60.f, 220.f, 60.f }, L"Cancel", BTN_GLASS)) Defer(CloseSheet);
    EndSheet();
}

// ---------------------------------------------------------------- camera
static const wchar_t kCamDeniedTitle[] = L"Windows is blocking the camera";
static const wchar_t kCamDeniedText[] =
    L"Turn on camera access in Windows Settings \x203A Privacy & security \x203A Camera, including access for desktop apps.";

static bool SameText(const std::wstring& a, const std::wstring& b) { return !a.empty() && _wcsicmp(a.c_str(), b.c_str()) == 0; }

// The camera the light on the home screen watches: the one picked in the camera sheet (the first USB
// camera seen is picked automatically), else the best one there is.
const CamDevice* RoomCamera(const CamStatus& s, std::wstring* missingName) {
    if (!g.cfg.camera.empty() || !g.cfg.cameraId.empty()) {
        const CamDevice* best = nullptr;
        int bestScore = 0;
        for (auto& d : s.cams) {
            bool id = SameText(d.hwid, g.cfg.cameraId), name = !g.cfg.camera.empty() && d.name == g.cfg.camera;
            int score = (id ? 2 : 0) + (name ? 2 : 0) + (d.link.empty() ? 0 : 1);
            if ((id || name) && score > bestScore) {
                bestScore = score;
                best = &d;
            }
        }
        if (!best && missingName) *missingName = g.cfg.camera.empty() ? g.cfg.cameraId : g.cfg.camera;
        return best;
    }
    for (auto& d : s.cams) if (d.external && !d.virt && !d.link.empty()) return &d;
    for (auto& d : s.cams) if (!d.virt && !d.link.empty()) return &d;
    for (auto& d : s.cams) if (!d.link.empty()) return &d;
    return s.cams.empty() ? nullptr : &s.cams[0];
}

static void CamProblemText(const CamDevice& d, std::wstring& title, std::wstring& text) {
    if (d.problem == 22) {          // CM_PROB_DISABLED
        title = Format(Tr(L"%s is turned off"), d.name.c_str());
        text = Tr(L"Turn it on again in Windows Settings \x203A Bluetooth & devices \x203A Cameras, or in Device Manager.");
    } else if (d.problem == 28) {   // CM_PROB_FAILED_INSTALL
        title = Format(Tr(L"%s has no driver"), d.name.c_str());
        text = Tr(L"Windows Update can usually install one. Or unplug the camera and plug it in again.");
    } else if (d.problem) {
        title = Format(Tr(L"%s has a problem (Code\x00A0%d)"), d.name.c_str(), d.problem);   // no-break space: "Code 10" stays together
        text = Tr(L"Unplug it and plug it in again. If that doesn't help, reinstall it in Device Manager.");
    } else {
        title = Format(Tr(L"%s is not available"), d.name.c_str());
        text = Tr(L"Unplug it and plug it in again.");
    }
}

static void CamFailText(int fail, HRESULT hr, std::wstring& title, std::wstring& text) {
    switch (fail) {
    case CAMFAIL_DENIED: title = Tr(kCamDeniedTitle); text = Tr(kCamDeniedText); break;
    case CAMFAIL_BUSY:
        title = Tr(L"Another app is using the camera");
        text = Tr(L"Close the other app or meeting, then press Test again.");
        break;
    case CAMFAIL_NOPICTURE:
        title = Tr(L"The camera sends no picture");
        text = Tr(L"Unplug the camera and plug it in again, or try another USB port.");
        break;
    case CAMFAIL_STOPPED:
        title = Tr(L"The picture stopped");
        text = Tr(L"Unplug the camera and plug it in again, or try another USB port.");
        break;
    case CAMFAIL_GONE:
        title = Tr(L"The camera was disconnected");
        text = Tr(L"Plug it in again. The test starts by itself when it is back.");
        break;
    case CAMFAIL_NOMF:
        title = Tr(L"Camera preview needs the Media Feature Pack");
        text = Tr(L"This edition of Windows has no video components. Add the Media Feature Pack in Settings \x203A System \x203A Optional features.");
        break;
    default:
        title = Tr(L"The camera could not start");
        text = HResultText(hr);
        break;
    }
}

int CameraLight(std::wstring* title, std::wstring* detail) {
    std::wstring t, d;
    int r = 1;
    const CamStatus& s = g.cams;
    if (!s.ready) {
        t = Tr(L"Looking for cameras\x2026");
        r = 0;
    } else if (s.denied) {
        t = Tr(kCamDeniedTitle);
        d = Tr(kCamDeniedText);
        r = 2;
    } else if (s.mfMissing) {
        CamFailText(CAMFAIL_NOMF, S_OK, t, d);
        r = 2;
    } else {
        std::wstring missing;
        const CamDevice* c = RoomCamera(s, &missing);
        if (!c && !missing.empty()) {
            t = Format(Tr(L"%s is not connected"), missing.c_str());
            d = Tr(L"Plug it in again, or pick another camera.");
            r = 2;
        } else if (!c) {
            t = Tr(L"No camera found");
            d = Tr(L"Plug in the USB camera. It appears here by itself.");
            r = 2;
        } else if (c->problem || c->link.empty()) {
            CamProblemText(*c, t, d);
            r = 2;
        } else if (g.camTest.result == 2 && g.camTest.name == c->name && g.camTest.fail != CAMFAIL_BUSY) {
            CamFailText(g.camTest.fail, g.camTest.hr, t, d);   // another app using it doesn't make it broken
            r = 2;
        } else {
            t = c->name;
        }
    }
    if (title) *title = t;
    if (detail) *detail = d;
    return r;
}

static void ResetCamPicture() {
    CamUi& c = g.cam;
    c.st = CamPreviewState();
    c.px.clear();
    c.px.shrink_to_fit();
    c.pw = c.ph = 0;
    c.seq = c.bmpSeq = 0;
    c.bmp.Reset();
    c.paused = false;
}

static void StartCameraTest() {
    CamUi& c = g.cam;
    g_camPreview.Stop();
    ResetCamPicture();
    c.startedAt = g.now;
    const CamDevice* d = g.cams.ready && !g.cams.denied && !g.cams.mfMissing ? RoomCamera(g.cams) : nullptr;
    c.name = d ? d->name : L"";
    c.link = d && !d->problem ? d->link : L"";
    if (c.link.empty()) return;   // nothing to show: the sheet says why
    if (c.demo) { DemoCamera(); return; }
    if (g.screenshot) return;     // screenshots never open a real camera
    g_camPreview.Start(c.link, g.hwnd);
    c.st.phase = CamPreviewState::Starting;
}

void CameraSheetGone() {
    if (g_camPreview.Active()) return;
    ResetCamPicture();
    g.cam.name.clear();
    g.cam.link.clear();
}

void OnCameraFrame() {
    g_camPreview.FrameShown();
    if (!g_camPreview.Active() || g.cam.demo) return;
    CamUi& c = g.cam;
    CamPreviewState prev = c.st;
    c.st = g_camPreview.State();
    int w = 0, h = 0;
    if (g_camPreview.TakeFrame(c.px, w, h, c.seq)) {
        c.pw = w;
        c.ph = h;
    }
    if (c.st.phase == CamPreviewState::Running && prev.phase != CamPreviewState::Running) g.camTest = { c.name, 1, 0, S_OK };
    if (c.st.phase == CamPreviewState::Failed && (prev.phase != CamPreviewState::Failed || prev.fail != c.st.fail))
        g.camTest = { c.name, 2, c.st.fail, c.st.hr };
    // these end the session on its own thread; keep the last state on screen
    if (c.st.phase == CamPreviewState::Failed && c.st.fail != CAMFAIL_NOPICTURE && c.st.fail != CAMFAIL_STOPPED) g_camPreview.Stop();
    RequestFrame();
}

void OnCamerasChanged() {
    CamStatus before = g.cams;
    g.cams = g_cams.Snapshot();
    if (g.cfg.camera.empty() && g.cfg.cameraId.empty()) {
        for (auto& d : g.cams.cams)
            if (d.external && !d.virt && !d.link.empty()) {   // the first USB camera seen becomes the room camera
                g.cfg.camera = d.name;
                g.cfg.cameraId = d.hwid;
                SaveConfig();
                break;
            }
    }
    // a camera that was plugged in again (or a new one) gets a fresh chance
    bool changed = before.cams.size() != g.cams.cams.size();
    for (size_t i = 0; !changed && i < before.cams.size(); i++)
        changed = before.cams[i].link != g.cams.cams[i].link || before.cams[i].problem != g.cams.cams[i].problem;
    if (changed && !g_camPreview.Active()) g.camTest = CamTest();
    if (g.sheet == Sheet::Camera && g.sheetOpen && !g.cam.demo && !g.cam.paused) {
        const CamDevice* d = RoomCamera(g.cams);
        std::wstring link = d && !d->problem && !g.cams.denied ? d->link : L"";
        if (link != g.cam.link || (!link.empty() && !g_camPreview.Active())) StartCameraTest();
    }
    RequestFrame();
}

void CameraAppActive(bool active) {
    if (g.sheet != Sheet::Camera || !g.sheetOpen || g.cam.demo) return;
    if (!active && g_camPreview.Active()) {   // let the app in front (Settings, a meeting) have the camera
        g_camPreview.Stop();
        ResetCamPicture();
        g.cam.paused = true;
    } else if (active && g.cam.paused) {
        StartCameraTest();
    }
    RequestFrame();
}

void DemoCamera() {   // SMPTE-style colour bars, so screenshots never show a real room
    CamUi& c = g.cam;
    const int w = 1280, h = 720;
    static const uint32_t top[7] = { 0xC0C0C0, 0xC0C000, 0x00C0C0, 0x00C000, 0xC000C0, 0xC00000, 0x0000C0 };
    static const uint32_t mid[7] = { 0x0000C0, 0x131313, 0xC000C0, 0x131313, 0x00C0C0, 0x131313, 0xC0C0C0 };
    static const uint32_t low[6] = { 0x00214C, 0xFFFFFF, 0x32006A, 0x131313, 0x090909, 0x1D1D1D };
    c.px.assign((size_t)w * h * 4, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t rgb = y < h * 2 / 3 ? top[x * 7 / w] : y < h * 3 / 4 ? mid[x * 7 / w] : low[x * 6 / w];
            uint8_t* p = &c.px[((size_t)y * w + x) * 4];
            p[0] = (uint8_t)(rgb & 255);
            p[1] = (uint8_t)((rgb >> 8) & 255);
            p[2] = (uint8_t)((rgb >> 16) & 255);
            p[3] = 255;
        }
    c.pw = w;
    c.ph = h;
    c.seq++;
    c.st = CamPreviewState();
    c.st.phase = CamPreviewState::Running;
    c.st.w = w;
    c.st.h = h;
    c.st.fps = 30.f;
    c.st.frames = 300;
    c.st.luma = 120.f;
    g.camTest = { c.name, 1, 0, S_OK };
}

static void CamSpinner(float cx, float cy, float r, D2D1_COLOR_F col) {
    float a = (float)std::fmod(g.now * 320.0, 360.0);
    gfx.Arc(cx, cy, r, a, a + 270.f, col, 4.f);
    ui.KeepAnimating("camspin");
}

static void SheetCamera() {
    CamUi& c = g.cam;
    const CamStatus& s = g.cams;
    std::wstring lightTitle, lightText;
    int light = CameraLight(&lightTitle, &lightText);
    Box card = BeginSheet(1400.f, 900.f, L"Camera", L"Check that the room camera works before the meeting");
    Box pv{ card.x + 44.f, card.y + 128.f, 896.f, 504.f };   // 16:9
    float rx = pv.r() + 40.f, rw = card.r() - 44.f - rx;
    const CamPreviewState& st = c.st;
    bool running = st.phase == CamPreviewState::Running && c.pw > 0 && !c.px.empty();
    bool showPicture = c.pw > 0 && !c.px.empty() && (running || !g.sheetOpen);

    // ---- the picture
    gfx.FillRect(pv, Rgb(0x050608), 22.f);
    if (showPicture) {
        if (!c.bmp || c.bmpGen != gfx.gen || c.bmpSeq != c.seq) {
            D2D1_SIZE_U sz = c.bmp ? c.bmp->GetPixelSize() : D2D1::SizeU(0, 0);
            if (c.bmp && c.bmpGen == gfx.gen && (int)sz.width == c.pw && (int)sz.height == c.ph)
                c.bmp->CopyFromMemory(nullptr, c.px.data(), (UINT32)c.pw * 4);
            else {
                c.bmp = gfx.BitmapFromBgrx(c.pw, c.ph, c.px.data());
                c.bmpGen = gfx.gen;
            }
            c.bmpSeq = c.seq;
        }
        if (c.bmp) {
            float k = std::min(pv.w / c.pw, pv.h / c.ph);
            Box dst{ pv.cx() - c.pw * k / 2, pv.cy() - c.ph * k / 2, c.pw * k, c.ph * k };
            ComPtr<ID2D1RoundedRectangleGeometry> geo;
            gfx.factory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(pv.rect(), 22.f, 22.f), &geo);
            if (geo) gfx.PushMask(geo.Get());
            D2D1_MATRIX_3X2_F saved;
            gfx.rt->GetTransform(&saved);
            if (g.cfg.cameraMirror) gfx.rt->SetTransform(D2D1::Matrix3x2F::Scale(-1.f, 1.f, D2D1::Point2F(dst.cx(), dst.cy())) * saved);
            gfx.rt->DrawBitmap(c.bmp.Get(), dst.rect(), 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            gfx.rt->SetTransform(saved);
            if (geo) gfx.PopMask();
        }
        if (running) {
            // "Working" chip, top left
            std::wstring ok = Tr(L"Working");
            float tw = gfx.TextWidth(ok, FACE_TEXT, 19.f, W_SEMIBOLD);
            Box chip{ pv.x + 18.f, pv.y + 18.f, tw + 58.f, 42.f };
            gfx.FillRect(chip, Rgb(0x000000, 0.55f), 21.f);
            gfx.FillCircle(chip.x + 22.f, chip.cy(), 7.f, g.th.success);
            gfx.Text(ok, { chip.x + 38.f, chip.y, tw + 10.f, chip.h }, FACE_TEXT, 19.f, W_SEMIBOLD, Rgb(0xFFFFFF));
            // a very dark picture: probably a closed privacy shutter or lens cover
            if (st.luma >= 0 && st.luma < 14.f && g.now - c.startedAt > 2.0) {
                Box hint{ pv.x + 40.f, pv.b() - 98.f, pv.w - 80.f, 70.f };
                gfx.FillRect(hint, Rgb(0x000000, 0.62f), 18.f);
                gfx.Glyph(0xE7BA, { hint.x + 14.f, hint.y, 44.f, hint.h }, 22.f, g.th.warn);
                gfx.Text(L"The picture is very dark. Is the privacy shutter or lens cover closed?",
                         { hint.x + 64.f, hint.y, hint.w - 80.f, hint.h }, FACE_TEXT, 19.f, W_SEMIBOLD, Rgb(0xFFFFFF), AL_LEFT, true, true);
            }
        }
    } else {
        std::wstring t, d;
        wchar_t glyph = 0xE960;
        bool spin = false, bad = false;
        if (c.paused) t = Tr(L"Paused while another app is in front");
        else if (st.phase == CamPreviewState::Starting) { t = Tr(L"Starting the camera\x2026"); spin = true; }
        else if (st.phase == CamPreviewState::Failed) { CamFailText(st.fail, st.hr, t, d); glyph = 0xE7BA; bad = st.fail != CAMFAIL_BUSY; }
        else if (light == 2) { t = lightTitle; d = lightText; glyph = 0xE7BA; bad = true; }
        else { t = Tr(L"Looking for cameras\x2026"); spin = true; }
        float cy = pv.y + pv.h * 0.36f;
        if (spin) CamSpinner(pv.cx(), cy, 34.f, Rgb(0xFFFFFF, 0.8f));
        else {
            gfx.FillCircle(pv.cx(), cy, 46.f, bad ? MulA(g.th.danger, 0.22f) : Rgb(0xFFFFFF, 0.10f));
            gfx.Glyph(glyph, { pv.cx() - 46.f, cy - 46.f, 92.f, 92.f }, 38.f, bad ? g.th.danger : Rgb(0xFFFFFF, 0.8f));
        }
        gfx.literal = true;   // the texts are already translated and may hold camera names
        gfx.Text(t, { pv.x + 40.f, cy + 64.f, pv.w - 80.f, 44.f }, FACE_TEXT, 27.f, W_SEMIBOLD, Rgb(0xFFFFFF), AL_CENTER);
        if (!d.empty())
            gfx.Text(d, { pv.x + 70.f, cy + 112.f, pv.w - 140.f, 90.f }, FACE_TEXT, 20.f, W_REGULAR, Rgb(0xFFFFFF, 0.72f),
                     AL_CENTER, false, true);
        gfx.literal = false;
    }
    gfx.StrokeRect(pv.inset(0.6f), MulA(g.th.glassStroke, 0.8f), 22.f, 1.2f);

    float y = pv.b() + 16.f;
    if (Toggle(Id("cam.mirror"), Row(pv.x, y, pv.w, L"Mirror image", L"Shows you as in a mirror, like meeting apps do", 120.f),
               g.cfg.cameraMirror))
        Defer([] { g.cfg.cameraMirror = !g.cfg.cameraMirror; SaveConfig(); });

    // ---- status (3 = in use by another app: not broken, so amber, and the home light stays green)
    bool busy = st.phase == CamPreviewState::Failed && st.fail == CAMFAIL_BUSY;
    int state = running ? 1 : busy ? 3 : st.phase == CamPreviewState::Failed ? 2 : st.phase == CamPreviewState::Starting ? 0 : light;
    Box sp{ rx, card.y + 128.f, rw, 156.f };
    Glass(sp, 24.f, 0, false);
    D2D1_COLOR_F sc = state == 1 ? g.th.success : state == 2 ? g.th.danger : state == 3 ? g.th.warn : g.th.text3;
    gfx.FillCircle(sp.x + 34.f, sp.y + 42.f, 10.f, sc);
    gfx.Text(state == 1 ? L"Working" : state == 2 ? L"Not working" : state == 3 ? L"In use" : L"Checking\x2026",
             { sp.x + 56.f, sp.y + 20.f, sp.w - 72.f, 44.f }, FACE_TEXT, 28.f, W_SEMIBOLD, g.th.text);
    std::wstring info;
    if (running) {
        info = c.name + L"\n" + (st.fps > 0.5f ? Format(L"%d \x00D7 %d \x00B7 %.0f fps", st.w, st.h, st.fps) : Format(L"%d \x00D7 %d", st.w, st.h));
    } else if (st.phase == CamPreviewState::Failed) {
        std::wstring t, d;
        CamFailText(st.fail, st.hr, t, d);
        info = c.name.empty() ? t : c.name + L"\n" + t;
    } else {
        info = state == 2 ? lightTitle : c.name;
    }
    gfx.literal = true;
    gfx.Text(info, { sp.x + 24.f, sp.y + 74.f, sp.w - 48.f, 70.f }, FACE_TEXT, 19.f, W_REGULAR, g.th.text2, AL_LEFT, false, true);
    gfx.literal = false;

    // ---- the cameras
    y = sp.b() + 22.f;
    Label(L"Cameras", rx, y, rw);
    gfx.Text(L"Tap the room camera. The light on the home screen follows it.", { rx, y + 30.f, rw, 48.f }, FACE_TEXT, 17.f,
             W_REGULAR, g.th.text3, AL_LEFT, false, true);
    y += 84.f;
    if (s.ready && s.cams.empty()) gfx.Text(L"No camera found", { rx, y, rw, 40.f }, FACE_TEXT, 21.f, W_SEMIBOLD, g.th.text2);
    const CamDevice* room = RoomCamera(s);
    const size_t maxRows = 4;
    for (size_t i = 0; i < s.cams.size() && i < maxRows; i++) {
        const CamDevice& d = s.cams[i];
        Box row{ rx, y, rw, 76.f };
        uint64_t id = Id("cam.row", i);
        ui.Add(id, row);
        bool sel = &d == room;
        float hov = ui.Anim(id ^ 0x51, ui.Highlighted(id) ? 1.f : 0.f, 16.f);
        Glass(row, 20.f, hov, false);
        if (sel) gfx.StrokeRect(row.inset(1.f), g.th.accent, 20.f, 2.f);
        float cx = row.x + 32.f, cy = row.cy();
        gfx.StrokeCircle(cx, cy, 12.f, sel ? g.th.accent : g.th.text3, 2.f);
        if (sel) gfx.FillCircle(cx, cy, 6.5f, g.th.accent);
        gfx.literal = true;
        gfx.Text(d.name, { row.x + 58.f, row.y + 9.f, row.w - 70.f, 32.f }, FACE_TEXT, 21.f, W_SEMIBOLD, g.th.text);
        gfx.literal = false;
        std::wstring kind = d.problem == 22 ? Tr(L"Turned off")
                          : d.problem == 28 ? Tr(L"No driver")
                          : d.problem       ? Format(Tr(L"Problem: Code %d"), d.problem)
                          : d.virt          ? Tr(L"Virtual camera")
                          : d.external      ? Tr(L"USB camera")
                                            : Tr(L"Built-in camera");
        gfx.literal = true;
        gfx.Text(kind, { row.x + 58.f, row.y + 41.f, row.w - 70.f, 26.f }, FACE_TEXT, 17.f, W_REGULAR,
                 d.problem || d.link.empty() ? g.th.danger : g.th.text3);
        gfx.literal = false;
        FocusRing(id, row, 20.f);
        if (ui.Clicked(id) && !sel) {
            std::wstring n = d.name, hw = d.hwid;
            Defer([n, hw] {
                g.cfg.camera = n;
                g.cfg.cameraId = hw;
                SaveConfig();
                g.camTest = CamTest();
                StartCameraTest();
            });
        }
        y += 84.f;
    }
    if (s.cams.size() > maxRows)
        gfx.Text(Format(Tr(L"and %d more"), (int)(s.cams.size() - maxRows)), { rx, y, rw, 30.f }, FACE_TEXT, 17.f, W_REGULAR, g.th.text3);

    // ---- footer
    float fy = card.b() - 40.f - 60.f, fx = card.x + 44.f;
    if (Button(Id("cam.again"), { fx, fy, 250.f, 60.f }, L"Test again", BTN_GLASS, 0xE72C))
        Defer([] { g.camTest = CamTest(); StartCameraTest(); });
    fx += 266.f;
    if (Button(Id("cam.winset"), { fx, fy, 290.f, 60.f }, L"Camera settings", BTN_GLASS, 0xE713))
        Defer([] { OpenUri(L"ms-settings:camera"); });
    fx += 306.f;
    bool denied = s.denied || (st.phase == CamPreviewState::Failed && st.fail == CAMFAIL_DENIED);
    if (denied && Button(Id("cam.privacy"), { fx, fy, 310.f, 60.f }, L"Privacy settings", BTN_ACCENT, 0xE72E))
        Defer([] { OpenUri(L"ms-settings:privacy-webcam"); });
    if (Button(Id("cam.done"), { card.r() - 44.f - 170.f, fy, 170.f, 60.f }, L"Done", BTN_ACCENT)) Defer(CloseSheet);
    EndSheet();
}

// ---------------------------------------------------------------- dispatch
void OnEditKey(int vk, HWND from) {
    if (vk == VK_ESCAPE) { CloseSheet(); return; }
    if (vk == VK_TAB) { edits.FocusNext(from, GetKeyState(VK_SHIFT) < 0); return; }
    if (vk != VK_RETURN) return;
    switch (g.sheet) {
    case Sheet::Rename: ApplyRename(); break;
    case Sheet::EditApp: SaveEditor(); break;
    case Sheet::Wifi:
        if (const WifiNet* w = SelectedNet()) { WifiNet copy = *w; ConnectNet(copy); }
        break;
    case Sheet::EditWidget:
        if (edits.IdOf(from) == Id("we.tzf")) {   // Enter in the search box picks the first match
            auto m = MatchingZones(g.wedit.tzFilter);
            if (!m.empty()) PickZone(*m[0]);
        } else {
            SaveWidgetEditor();
        }
        break;
    default: SetFocus(g.hwnd); break;
    }
    RequestFrame();
}

void DrawSheets() {
    if (g.sheet == Sheet::None) return;
    switch (g.sheet) {
    case Sheet::Wifi: SheetWifi(); break;
    case Sheet::Network: SheetNetwork(); break;
    case Sheet::Settings: SheetSettings(); break;
    case Sheet::Power: SheetPower(); break;
    case Sheet::AddApp: SheetAdd(); break;
    case Sheet::EditApp: SheetEdit(); break;
    case Sheet::Rename: SheetRename(); break;
    case Sheet::Pin: SheetPin(); break;
    case Sheet::AddWidget: SheetAddWidget(); break;
    case Sheet::EditWidget: SheetEditWidget(); break;
    case Sheet::Camera: SheetCamera(); break;
    default: break;
    }
    ui.layer = 0;
}
