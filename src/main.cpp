// RoomHome - full-screen home screen for meeting-room PCs.
#include "app.h"
#include "lang.h"
#include <shellapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <dbt.h>
#include <share.h>

App g;

static ComPtr<ID2D1HwndRenderTarget> s_rt;
static LARGE_INTEGER s_freq;
static double s_lastFrame = 0;
static bool s_mouseTracking = false, s_swallowUp = false, s_swallowTouch = false;
static POINT s_lastMouse{ -1, -1 };
static uint64_t s_toastSeq = 0;
static RECT s_windowRect{};
static bool s_noActivate = false;

static FILE* s_log = nullptr;
static void Log(const char* fmt, ...) {
    if (!s_log) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(s_log, "%10.3f ", GetTickCount64() / 1000.0);
    vfprintf(s_log, fmt, ap);
    fputc('\n', s_log);
    fflush(s_log);
    va_end(ap);
}

static double Now() {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)s_freq.QuadPart;
}

// ---------------------------------------------------------------- shared helpers
void RequestFrame() {
    if (g.hwnd) InvalidateRect(g.hwnd, nullptr, FALSE);
}

void Defer(std::function<void()> fn) { g.deferred.push_back(std::move(fn)); }

void SaveConfig() {
    if (g.screenshot) return;
    if (!ConfigSave(g.cfg)) ShowToast(Format(Tr(L"Couldn't save the settings to %s"), ConfigPath().c_str()), 2);
}

void ApplyLanguage() {
    int l = LangDetect(g.cfg.language);
    if (l == g_lang) return;
    g_lang = l;
    gfx.ClearTextCaches();   // text formats carry the locale (Thai line breaking)
    edits.Clear();           // placeholders are set when a field is created; they come back next frame
    RequestFrame();
}

void ShowToast(const std::wstring& text, int kind, const std::wstring& actionLabel, std::function<void()> action) {
    Toast t;
    t.id = ++s_toastSeq;
    t.text = text;
    t.kind = kind;
    t.born = g.now;
    t.life = action ? 6.0 : (kind == 2 ? 6.0 : 3.5);
    t.actionLabel = actionLabel;
    t.action = std::move(action);
    ui.AnimSet(Id("toast", t.id), 0.f);
    g.toasts.push_back(std::move(t));
    while (g.toasts.size() > 3) g.toasts.erase(g.toasts.begin());
    RequestFrame();
}

bool IsLight() { return g.cfg.theme == 2 || (g.cfg.theme == 0 && g.systemLight); }

void ApplyKeepAwake() {
    if (g.screenshot) return;
    SetThreadExecutionState(ES_CONTINUOUS | (g.cfg.keepAwake ? (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED) : 0));
}

void RefreshAutostart() {
    g.autostartUser = AutostartUser();
    g.autostartMachine = AutostartMachine();
}

void LaunchItem(const DockItem& it) {
    if (g.lastLaunchUid == it.uid && g.now - g.lastLaunch < 1.5) return;   // double taps
    g.lastLaunch = g.now;
    g.lastLaunchUid = it.uid;
    LaunchAsync(it, g.hwnd, g.dryRun);
}

ID2D1Bitmap* IconFor(const DockItem& it) {
    auto k = g.iconKeys.find(it.uid);
    std::wstring key;
    if (k == g.iconKeys.end()) {
        key = IconSourceFor(it);
        g.iconKeys[it.uid] = key;
    } else {
        key = k->second;
    }
    if (key.empty()) return nullptr;
    IconEntry& e = g.icons[key];
    if (e.failed) return nullptr;
    if (e.px.empty()) {
        if (g.screenshot) {
            IconMsg m;
            LoadIconSync(key, m);
            if (!m.ok) { e.failed = true; return nullptr; }
            e.w = m.w; e.h = m.h; e.px = std::move(m.px);
        } else {
            if (!e.loading && g.hwnd) { e.loading = true; LoadIconAsync(key, g.hwnd); }
            return nullptr;
        }
    }
    if (!e.bmp || e.gen != gfx.gen) {
        e.bmp = gfx.BitmapFromPixels(e.w, e.h, e.px.data());
        e.gen = gfx.gen;
    }
    return e.bmp.Get();
}

void InvalidateIcon(uint32_t uid) { g.iconKeys.erase(uid); }

static void RefreshAudio() {
    g.audioOk = g_audio.Ok();
    g.volume = g.audioOk ? g_audio.Level() : 0.f;
    g.muted = g.audioOk && g_audio.Muted();
    g.audioDevice = g_audio.Device();
}

// ---------------------------------------------------------------- layout + rendering
static void UpdateLayout(float w, float h) {
    g.W = std::max(1.f, w);
    g.H = std::max(1.f, h);
    g.s = std::min(g.H / 1080.f, g.W / 1720.f);
    if (g.s <= 0.01f) g.s = 1.f;
    g.vw = g.W / g.s;
    g.vh = g.H / g.s;
}

static HRESULT RenderFrame(ID2D1RenderTarget* rt, float dt) {
    ui.BeginFrame(g.now, dt);
    GetLocalTime(&g.st);
    if (g.st.wSecond != g.shownSec) {
        g.prevSec = g.shownSec;
        g.shownSec = g.st.wSecond;
        g.secChangedAt = g.now;
    }
    // Light/dark switch. Blending the two palettes passes through mid-grey text on a mid-tone
    // background (the UI all but vanished half-way), so the new theme is revealed as a growing
    // circle drawn over the old one instead; every pixel is always fully one theme or the other.
    bool target = IsLight();
    if (!g.themeInited) { g.lightShown = target; g.themeInited = true; }
    if (g.themeAnimating) {
        g.themeProg = Saturate((float)((g.now - g.themeStart) / 0.75));
        if (g.themeProg >= 1.f) { g.lightShown = g.lightTo; g.themeAnimating = false; }
    }
    if (!g.themeAnimating && target != g.lightShown) {
        g.themeAnimating = true;
        g.lightTo = target;
        g.themeStart = g.now;
        g.themeProg = 0;
    }
    g.ambient = ui.Anim(Id("ambient"), g.ambientOn ? 1.f : 0.f, 2.2f);
    if (g.sheetOpen) {
        g.sheetT = ui.Anim(Id("sheetT"), 1.f, 13.f);
    } else {
        // closing runs a short linear 0.18 s: an exponential ease lingers near transparent,
        // which showed the clock through the fading card for a third of a second
        g.sheetT = std::max(0.f, g.sheetT - dt / 0.18f);
        ui.AnimSet(Id("sheetT"), g.sheetT);
        if (g.sheetT > 0.f) ui.KeepAnimating("sheetclose");
    }
    if (!g.sheetOpen && g.sheetT <= 0.002f) {
        if (g.sheet == Sheet::Camera) CameraSheetGone();
        g.sheet = Sheet::None;
    }

    auto pass = [&](bool light) {
        g.themeT = light ? 1.f : 0.f;
        g.th = MakeTheme(g.themeT, g.cfg.accent);
        rt->SetTransform(D2D1::Matrix3x2F::Scale(g.s, g.s));
        DrawBackground();
        rt->SetTransform(D2D1::Matrix3x2F::Scale(g.s, g.s) * D2D1::Matrix3x2F::Translation(g.shiftX, g.shiftY));
        // ambient mode: the home screen fades out first, then the clock fades in (no double exposure)
        DrawHome(1.f - Saturate(g.ambient * 2.f));
        DrawAmbient();
        DrawSheets();
        DrawToasts();
        DrawShutdown();
    };
    rt->BeginDraw();
    rt->Clear(D2D1::ColorF(0, 0, 0, 1));
    pass(g.lightShown);
    if (g.themeAnimating) {
        float ox = g.themeOriginX * g.s + g.shiftX, oy = g.themeOriginY * g.s + g.shiftY;
        float farthest = 0;
        for (float cx : { 0.f, g.W })
            for (float cy : { 0.f, g.H }) farthest = std::max(farthest, std::hypot(cx - ox, cy - oy));
        float e = g.themeProg < 0.5f ? 4 * g.themeProg * g.themeProg * g.themeProg
                                     : 1 - std::pow(-2 * g.themeProg + 2, 3.f) / 2;   // ease in-out cubic
        ComPtr<ID2D1EllipseGeometry> circle;
        if (SUCCEEDED(gfx.factory->CreateEllipseGeometry(D2D1::Ellipse(D2D1::Point2F(ox, oy), farthest * e + 1.f, farthest * e + 1.f), &circle))) {
            rt->SetTransform(D2D1::Matrix3x2F::Identity());
            gfx.PushMask(circle.Get());
            bool sup = ui.suppress;
            ui.suppress = true;   // the first pass already registered everything clickable
            ui.freeze = true;     // and advanced the animations
            pass(g.lightTo);
            ui.freeze = false;
            ui.suppress = sup;
            rt->SetTransform(D2D1::Matrix3x2F::Identity());
            gfx.PopMask();
        }
        ui.KeepAnimating("theme");
    }
    HRESULT hr = rt->EndDraw();
    ui.EndFrame();
    return hr;
}

static void RunDeferred() {
    for (int guard = 0; guard < 4 && !g.deferred.empty(); guard++) {
        auto list = std::move(g.deferred);
        g.deferred.clear();
        for (auto& f : list) f();
    }
}

static bool EnsureTarget() {
    if (s_rt) return true;
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f);
    D2D1_HWND_RENDER_TARGET_PROPERTIES hp = D2D1::HwndRenderTargetProperties(
        g.hwnd, D2D1::SizeU(std::max<LONG>(1, rc.right - rc.left), std::max<LONG>(1, rc.bottom - rc.top)), D2D1_PRESENT_OPTIONS_NONE);
    if (FAILED(gfx.factory->CreateHwndRenderTarget(p, hp, &s_rt))) return false;
    s_rt->SetDpi(96.f, 96.f);
    gfx.Bind(s_rt.Get());
    return true;
}

static void RenderMain() {
    if (!EnsureTarget()) return;
    double t = Now();
    float dt = (float)std::min(0.1, std::max(0.0, t - s_lastFrame));
    s_lastFrame = t;
    g.now = t;
    HRESULT hr = RenderFrame(s_rt.Get(), dt);
    Log("frame hr=%08x sheet=%d sheetT=%.3f anim=%d why=%s key=%llx %.4f->%.4f hot=%llx", (unsigned)hr, (int)g.sheet, g.sheetT,
        ui.animating ? 1 : 0, ui.dbgWhy, (unsigned long long)ui.dbgKey, ui.dbgValue, ui.dbgTarget, (unsigned long long)ui.hot);
    if (hr == D2DERR_RECREATE_TARGET) {
        gfx.Unbind();
        s_rt.Reset();
        RequestFrame();
    }
    bool editsVisible = g.sheetOpen && g.sheetT > 0.97f && g.ambient < 0.01f;
    edits.Sync(g.s, g.shiftX, g.shiftY, editsVisible, ToColorRef(g.th.text), ToColorRef(g.th.field), 24.f * g.s);
    bool hadDeferred = !g.deferred.empty();
    RunDeferred();
    if (ui.animating || hadDeferred || ui.pointerDown) InvalidateRect(g.hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------- window placement
static BOOL CALLBACK MonEnum(HMONITOR m, HDC, LPRECT, LPARAM lp) {
    MONITORINFO mi{ sizeof(mi) };
    if (GetMonitorInfoW(m, &mi)) {
        auto* v = (std::vector<std::pair<bool, RECT>>*)lp;
        v->push_back({ (mi.dwFlags & MONITORINFOF_PRIMARY) != 0, mi.rcMonitor });
    }
    return TRUE;
}

static RECT TargetRect() {
    if (g.windowed) return s_windowRect;
    std::vector<std::pair<bool, RECT>> mons;
    EnumDisplayMonitors(nullptr, nullptr, MonEnum, (LPARAM)&mons);
    std::stable_sort(mons.begin(), mons.end(), [](auto& a, auto& b) {
        if (a.first != b.first) return a.first;
        return a.second.left < b.second.left;
    });
    if (g.cfg.monitor > 0 && g.cfg.monitor <= (int)mons.size()) return mons[g.cfg.monitor - 1].second;
    if (!mons.empty()) return mons[0].second;
    return RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

static void PlaceWindow() {
    RECT r = TargetRect();
    SetWindowPos(g.hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void ScheduleClock() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    SetTimer(g.hwnd, 1, 1000 - st.wMilliseconds + 3, nullptr);
}

static void OnClockTick() {
    if (g.cfg.idleMinutes > 0 && g.sheet == Sheet::None && !g.editMode && !g.ambientOn) {
        LASTINPUTINFO li{ sizeof(li) };
        if (GetLastInputInfo(&li) && GetTickCount() - li.dwTime > (DWORD)g.cfg.idleMinutes * 60000u) g.ambientOn = true;
    }
    // Keep static pixels from burning in - but only while nobody is using the screen, so the
    // 2-pixel move is never seen as a twitch.
    static ULONGLONG lastShift = GetTickCount64();
    LASTINPUTINFO lii{ sizeof(lii) };
    bool idle = GetLastInputInfo(&lii) && GetTickCount() - lii.dwTime > 60000;
    if (idle && g.sheet == Sheet::None && GetTickCount64() - lastShift > 180000) {
        lastShift = GetTickCount64();
        g.shiftPhase++;
        g.shiftX = std::round(2.f * std::cos(g.shiftPhase * 0.9f));
        g.shiftY = std::round(2.f * std::sin(g.shiftPhase * 1.3f));
    }
    // the camera test closes itself after 3 quiet minutes, so a forgotten test never keeps the camera on
    if (g.sheet == Sheet::Camera && g.sheetOpen && g.now - g.cam.startedAt > 180.0) {
        LASTINPUTINFO lc{ sizeof(lc) };
        if (GetLastInputInfo(&lc) && GetTickCount() - lc.dwTime > 180000) CloseSheet();
    }
    if (g.calOffset != 0 && g.now - g.calTouched > 60.0) g.calOffset = 0;
    for (auto it = g.photos.begin(); it != g.photos.end();) {   // drop pictures nobody has drawn for a minute
        if (!it->second.loading && g.now - it->second.lastUsed > 60.0) it = g.photos.erase(it);
        else ++it;
    }
}

// ---------------------------------------------------------------- input
static void ToVirtual(int x, int y, float& vx, float& vy) {
    vx = (x - g.shiftX) / g.s;
    vy = (y - g.shiftY) / g.s;
}

static bool WakeFromAmbient() {
    if (g.ambientOn || g.ambient > 0.05f) {
        g.ambientOn = false;
        RequestFrame();
        return true;
    }
    return false;
}

static bool IsTouchMouse() { return (GetMessageExtraInfo() & 0xFFFFFF00) == 0xFF515700; }

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        BOOL off = FALSE;
        SetWindowFeedbackSetting(hwnd, FEEDBACK_TOUCH_PRESSANDHOLD, 0, sizeof(off), &off);
        SetWindowFeedbackSetting(hwnd, FEEDBACK_TOUCH_RIGHTTAP, 0, sizeof(off), &off);
        SetWindowFeedbackSetting(hwnd, FEEDBACK_PEN_PRESSANDHOLD, 0, sizeof(off), &off);
        SetWindowFeedbackSetting(hwnd, FEEDBACK_PEN_RIGHTTAP, 0, sizeof(off), &off);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        if (!IsIconic(hwnd)) RenderMain();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        UpdateLayout((float)LOWORD(lp), (float)HIWORD(lp));
        if (s_rt) s_rt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        RequestFrame();
        return 0;
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED:
        if (!g.windowed) PlaceWindow();
        RequestFrame();
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            OnClockTick();
            ScheduleClock();
            RequestFrame();
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && (HWND)wp == hwnd) {
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP: {
        UINT32 pid = GET_POINTERID_WPARAM(wp);
        POINTER_INPUT_TYPE type = PT_POINTER;
        if (!GetPointerType(pid, &type) || (type != PT_TOUCH && type != PT_PEN)) break;   // mouse: legacy messages
        if (!IS_POINTER_PRIMARY_WPARAM(wp)) return 0;
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &pt);
        float vx, vy;
        ToVirtual(pt.x, pt.y, vx, vy);
        if (msg == WM_POINTERDOWN) {
            if (WakeFromAmbient()) { s_swallowTouch = true; return 0; }
            if (edits.AnyFocused()) SetFocus(hwnd);
            ui.PointerMove(vx, vy, true);
            ui.PointerDown(vx, vy, true, false);
        } else if (msg == WM_POINTERUPDATE) {
            if (s_swallowTouch) return 0;
            if (IS_POINTER_INCONTACT_WPARAM(wp)) ui.PointerMove(vx, vy, true);
        } else {
            if (s_swallowTouch) { s_swallowTouch = false; return 0; }
            ui.PointerUp(vx, vy, true, false);
        }
        RequestFrame();
        return 0;
    }
    case WM_POINTERCAPTURECHANGED:
        if (ui.pointerDown && ui.touch) {
            ui.activeConsumed = true;
            ui.PointerUp(ui.px, ui.py, true, false);
            RequestFrame();
        }
        break;

    case WM_MOUSEMOVE: {
        if (IsTouchMouse()) break;
        POINT p{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (p.x == s_lastMouse.x && p.y == s_lastMouse.y) return 0;
        bool firstMove = s_lastMouse.x < 0;
        s_lastMouse = p;
        if (!s_mouseTracking) {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            s_mouseTracking = true;
        }
        if (!firstMove && g.ambientOn) { WakeFromAmbient(); return 0; }
        float vx, vy;
        ToVirtual(p.x, p.y, vx, vy);
        ui.PointerMove(vx, vy, false);
        RequestFrame();
        return 0;
    }
    case WM_MOUSELEAVE:
        s_mouseTracking = false;
        s_lastMouse = { -1, -1 };
        ui.PointerLeave();
        RequestFrame();
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        if (IsTouchMouse()) break;
        if (WakeFromAmbient()) { s_swallowUp = true; return 0; }
        if (edits.AnyFocused()) SetFocus(hwnd);
        float vx, vy;
        ToVirtual(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), vx, vy);
        if (msg == WM_LBUTTONDOWN) SetCapture(hwnd);
        ui.PointerMove(vx, vy, false);
        ui.PointerDown(vx, vy, false, msg == WM_RBUTTONDOWN);
        Log("mouse down %.0f,%.0f active=%llx", vx, vy, (unsigned long long)ui.active);
        RequestFrame();
        return 0;
    }
    case WM_LBUTTONUP: {
        if (IsTouchMouse()) break;
        if (GetCapture() == hwnd) ReleaseCapture();
        if (s_swallowUp) { s_swallowUp = false; return 0; }
        float vx, vy;
        ToVirtual(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), vx, vy);
        Log("mouse up %.0f,%.0f active=%llx hit=%llx moved=%d consumed=%d", vx, vy, (unsigned long long)ui.active,
            (unsigned long long)ui.HitTest(vx, vy), ui.moved ? 1 : 0, ui.activeConsumed ? 1 : 0);
        ui.PointerUp(vx, vy, false, false);
        RequestFrame();
        return 0;
    }
    case WM_MOUSEWHEEL:
        if (WakeFromAmbient()) return 0;
        ui.Wheel(GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA);
        RequestFrame();
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        if (wp == VK_F4 && msg == WM_SYSKEYDOWN) break;   // Alt+F4 -> WM_CLOSE (handled below)
        if (WakeFromAmbient()) return 0;
        bool repeat = (lp & (1 << 30)) != 0;
        if (wp == VK_ESCAPE) {
            if (g.sheet != Sheet::None && g.sheetOpen) CloseSheet();
            else if (g.editMode) { g.editMode = false; g.dragUid = 0; }
            RequestFrame();
            return 0;
        }
        if (g.sheet == Sheet::Pin) return 0;   // digits arrive as WM_CHAR
        if (ui.Key((int)wp, true, repeat)) { RequestFrame(); return 0; }
        break;
    }
    case WM_KEYUP:
        if (ui.Key((int)wp, false, false)) { RequestFrame(); return 0; }
        break;
    case WM_CHAR:
        if (g.sheet == Sheet::Pin) { OnPinChar((wchar_t)wp); return 0; }
        return 0;

    case WM_CTLCOLOREDIT: {
        HBRUSH b = edits.CtlColor((HDC)wp, (HWND)lp);
        if (b) return (LRESULT)b;
        break;
    }
    case WM_COMMAND:
        if (HIWORD(wp) == EN_CHANGE) { edits.OnChange((HWND)lp); RequestFrame(); }
        else if (HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == EN_KILLFOCUS) RequestFrame();
        return 0;
    case WM_APP_EDITKEY:
        OnEditKey((int)wp, (HWND)lp);
        return 0;

    case WM_APP_NET:
        g.net = g_net.Snapshot();
        RequestFrame();
        return 0;
    case WM_APP_WIFIEVENT: {
        std::unique_ptr<WifiEvent> e((WifiEvent*)lp);
        OnWifiEvent(*e);
        return 0;
    }
    case WM_APP_AUDIO:
        RefreshAudio();
        RequestFrame();
        return 0;
    case WM_APP_AUDIODEV:
        g_audio.Reacquire();
        RefreshAudio();
        RequestFrame();
        return 0;
    case WM_APP_LAUNCHED: {
        std::unique_ptr<LaunchMsg> m((LaunchMsg*)lp);
        ShowToast(m->text, m->ok ? 1 : 2);
        return 0;
    }
    case WM_APP_ICON: {
        std::unique_ptr<IconMsg> m((IconMsg*)lp);
        IconEntry& e = g.icons[m->key];
        e.loading = false;
        if (m->ok) { e.w = m->w; e.h = m->h; e.px = std::move(m->px); e.bmp.Reset(); }
        else e.failed = true;
        RequestFrame();
        return 0;
    }
    case WM_APP_PHOTO: {
        std::unique_ptr<PhotoMsg> m((PhotoMsg*)lp);
        auto it = g.photos.find(m->key);
        if (it != g.photos.end()) {
            PhotoEntry& e = it->second;
            e.loading = false;
            if (m->ok) { e.w = m->w; e.h = m->h; e.px = std::move(m->px); e.bmp.Reset(); }
            else e.failed = true;
        }
        RequestFrame();
        return 0;
    }
    case WM_APP_CAMERA:
        OnCamerasChanged();
        return 0;
    case WM_APP_CAMFRAME: {
        if (wp == 1) { Log("camera: released"); return 0; }
        CamPreviewState before = g.cam.st;
        OnCameraFrame();
        const CamPreviewState& st = g.cam.st;
        if (st.phase != before.phase || st.fail != before.fail)
            Log("camera: phase=%d fail=%d hr=%08x size=%dx%d first=%.0fms", (int)st.phase, st.fail, (unsigned)st.hr, st.w, st.h,
                st.firstFrameMs);
        if (st.frames >= 90 && before.frames < 90) Log("camera: %u pictures, %.1f fps, brightness %.0f", st.frames, st.fps, st.luma);
        return 0;
    }
    case WM_DEVICECHANGE:   // a camera may have been plugged in or removed
        if (wp == DBT_DEVNODES_CHANGED || wp == DBT_DEVICEARRIVAL || wp == DBT_DEVICEREMOVECOMPLETE) g_cams.Rescan();
        return TRUE;
    case WM_ACTIVATEAPP:
        CameraAppActive(wp != 0);
        break;
    case WM_SETTINGCHANGE:
        if (lp && wcscmp((const wchar_t*)lp, L"ImmersiveColorSet") == 0) {
            g.systemLight = SystemPrefersLight();
            RequestFrame();
        }
        break;
    case WM_ACTIVATE:
        RequestFrame();
        break;

    case WM_APP_QUIT:
        g.allowClose = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        if (g.allowClose || g.windowed) { DestroyWindow(hwnd); return 0; }
        ShowToast(L"RoomHome stays open on this screen. To close it, use Settings \x203A Exit RoomHome.", 0);
        return 0;
    case WM_QUERYENDSESSION:
        return TRUE;
    case WM_ENDSESSION:
        if (wp) SaveConfig();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        g_camPreview.Stop();
        edits.Clear();
        if (!g.screenshot) ConfigSave(g.cfg);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------- test modes
static void DemoData() {
    NetInfo& n = g.net;
    n = NetInfo();
    n.wifiPresent = true;
    n.wifiConnected = true;
    n.wifiSsid = L"Office-5G";
    n.wifiQuality = 82;
    n.wifiIp = L"10.20.31.77";
    n.wifiGateway = L"10.20.31.1";
    n.wifiDns = L"10.20.0.10, 10.20.0.11";
    n.wifiMac = L"3C-21-9C-4A-10-EE";
    n.wifiAdapter = L"Intel(R) Wi-Fi 7 BE200";
    n.lanPresent = true;
    n.lanConnected = true;
    n.lanSpeed = 1000000000ULL;
    n.lanIp = L"10.20.30.41";
    n.lanGateway = L"10.20.30.1";
    n.lanDns = L"10.20.0.10, 10.20.0.11";
    n.lanMac = L"00-E0-4C-68-01-2A";
    n.lanAdapter = L"Realtek PCIe GbE Family Controller";
    n.internet = true;
    n.netsValid = true;
    const struct { const char* s; int q; bool sec, conn, prof; } demo[] = {
        { "Office-5G", 82, true, true, true }, { "Office-Guest", 74, false, false, false },
        { "MeetingRoom-Cast", 61, true, false, true }, { "Corp-Secure", 55, true, false, false },
        { "Boardroom-AP", 47, true, false, false }, { "Cafe Free WiFi", 21, false, false, false },
    };
    for (auto& d : demo) {
        WifiNet w;
        w.ssidRaw = d.s;
        w.ssid = Utf8ToW(d.s);
        w.quality = d.q;
        w.secure = d.sec;
        w.connected = d.conn;
        w.hasProfile = d.prof;
        w.profile = w.ssid;
        w.auth = d.sec ? 7 : 1;
        w.supported = std::string(d.s) != "Corp-Secure";
        n.nets.push_back(w);
    }
    g.audioOk = true;
    g.volume = 0.45f;
    g.muted = false;

    CamStatus& cs = g.cams;
    cs = CamStatus();
    cs.ready = true;
    cs.seq = 1;
    CamDevice usb;
    usb.name = L"USB Conference Camera";
    usb.link = L"demo:usb";
    usb.hwid = L"DEMO\\USB-CAMERA";
    usb.external = true;
    CamDevice builtIn;
    builtIn.name = L"Integrated Camera";
    builtIn.link = L"demo:builtin";
    builtIn.hwid = L"DEMO\\BUILT-IN-CAMERA";
    cs.cams = { usb, builtIn };
    g.cfg.camera = usb.name;
    g.cfg.cameraId = usb.hwid;
    g.cam.demo = true;
    g.camTest = { usb.name, 1, 0, S_OK };
}

static bool SavePng(IWICBitmap* bmp, const std::wstring& path) {
    ComPtr<IWICStream> st;
    ComPtr<IWICBitmapEncoder> enc;
    ComPtr<IWICBitmapFrameEncode> fr;
    if (FAILED(gfx.wic->CreateStream(&st)) || FAILED(st->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) return false;
    if (FAILED(gfx.wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) ||
        FAILED(enc->Initialize(st.Get(), WICBitmapEncoderNoCache)))
        return false;
    if (FAILED(enc->CreateNewFrame(&fr, nullptr)) || FAILED(fr->Initialize(nullptr))) return false;
    UINT w, h;
    bmp->GetSize(&w, &h);
    fr->SetSize(w, h);
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    fr->SetPixelFormat(&fmt);
    ComPtr<IWICFormatConverter> conv;
    gfx.wic->CreateFormatConverter(&conv);
    conv->Initialize(bmp, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
    if (FAILED(fr->WriteSource(conv.Get(), nullptr)) || FAILED(fr->Commit()) || FAILED(enc->Commit())) return false;
    return true;
}

struct ShotOpts {
    std::wstring out, sheet, theme, camState;
    int w = 1920, h = 1080;
    bool edit = false, ambient = false, demo = false;
    float hoverX = -1, hoverY = -1;
    int focusTab = 0;
};

static int RunScreenshot(const ShotOpts& o) {
    g.screenshot = true;
    edits.enabled = false;
    ComPtr<IWICBitmap> bmp;
    if (FAILED(gfx.wic->CreateBitmap(o.w, o.h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bmp))) return 2;
    D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f);
    ComPtr<ID2D1RenderTarget> rt;
    if (FAILED(gfx.factory->CreateWicBitmapRenderTarget(bmp.Get(), p, &rt))) return 3;
    gfx.Bind(rt.Get());
    UpdateLayout((float)o.w, (float)o.h);
    if (o.demo) DemoData();
    else {
        g_net.Start(nullptr);
        g_cams.Start(nullptr);
        g_net.WaitFirst(4000);
        g_cams.WaitFirst(4000);
        g.net = g_net.Snapshot();
        g.cams = g_cams.Snapshot();
        g_audio.Init(nullptr);
        RefreshAudio();
    }
    RefreshAutostart();
    if (o.theme == L"dark") g.cfg.theme = 1;
    if (o.theme == L"light") g.cfg.theme = 2;
    g.editMode = o.edit;
    g.ambientOn = o.ambient;
    g.now = Now();
    // --camstate (with --demo): how the camera light and sheet look when something is wrong
    const std::wstring& cs = o.camState;
    if (o.demo && !cs.empty() && !g.cams.cams.empty()) {
        if (cs == L"none") { g.cams.cams.clear(); g.cfg.camera.clear(); g.cfg.cameraId.clear(); }
        else if (cs == L"missing") g.cams.cams.erase(g.cams.cams.begin());
        else if (cs == L"blocked") g.cams.denied = true;
        else if (cs == L"problem") { g.cams.cams[0].problem = 10; g.cams.cams[0].link.clear(); }
        else if (cs == L"off") { g.cams.cams[0].problem = 22; g.cams.cams[0].link.clear(); }
        if (cs != L"busy" && cs != L"dark" && cs != L"starting") g.camTest = CamTest();
    }
    if (!o.sheet.empty()) {
        if (o.sheet == L"wifi") OpenSheet(Sheet::Wifi);
        else if (o.sheet == L"network") OpenSheet(Sheet::Network);
        else if (o.sheet == L"settings") OpenSheet(Sheet::Settings);
        else if (o.sheet == L"power") OpenSheet(Sheet::Power);
        else if (o.sheet == L"add") OpenSheet(Sheet::AddApp);
        else if (o.sheet == L"rename") OpenSheet(Sheet::Rename);
        else if (o.sheet == L"pin") { g.cfg.adminPin = L"2468"; g.pin.entered = L"24"; OpenSheet(Sheet::Pin); g.pin.entered = L"24"; }
        else if (o.sheet == L"edit" && !g.cfg.dock.empty()) OpenEditor(0, g.cfg.dock[0], false);
        else if (o.sheet == L"editweb" && g.cfg.dock.size() > 2) OpenEditor(2, g.cfg.dock[2], false);
        else if (o.sheet == L"newprog") { DockItem it = ItemFromPreset(kPresets[kPresetCount - 1]); OpenEditor(-1, it, true); }
        else if (o.sheet == L"addwidget") OpenSheet(Sheet::AddWidget);
        else if (o.sheet == L"camera") OpenSheet(Sheet::Camera);
        else if (o.sheet.rfind(L"widget", 0) == 0) OpenWidgetEditor(_wtoi(o.sheet.c_str() + 6));
        if (o.sheet == L"wifi" && o.demo && g.net.nets.size() > 4) g.wifi.selected = g.net.nets[4].ssidRaw;
    }
    if (o.demo && g.sheet == Sheet::Camera) {
        CamPreviewState& st = g.cam.st;
        if (cs == L"busy" || cs == L"nopicture") {
            st = CamPreviewState();
            st.phase = CamPreviewState::Failed;
            st.fail = cs == L"busy" ? CAMFAIL_BUSY : CAMFAIL_NOPICTURE;
            g.camTest = { g.cam.name, 2, st.fail, S_OK };
        } else if (cs == L"starting") {
            st = CamPreviewState();
            st.phase = CamPreviewState::Starting;
        } else if (cs == L"dark") {
            for (size_t i = 0; i + 3 < g.cam.px.size(); i += 4) { g.cam.px[i] /= 24; g.cam.px[i + 1] /= 24; g.cam.px[i + 2] /= 24; }
            g.cam.seq++;
            st.luma = 5.f;
            g.cam.startedAt = g.now - 10.0;
        }
    }
    if (o.sheet == L"toast") ShowToast(Format(Tr(L"%s isn't installed here, so it opened on the web"), L"Zoom"), 0);
    if (o.sheet == L"toast") ShowToast(Format(Tr(L"Removed %s"), L"Webex"), 0, L"Undo", [] {});
    if (o.hoverX >= 0) { ui.PointerMove(o.hoverX, o.hoverY, false); }
    HRESULT hr = S_OK;
    for (int i = 0; i < 8; i++) {
        g.now += 0.6;
        if (o.hoverX >= 0) ui.PointerMove(o.hoverX, o.hoverY, false);
        if (i == 2 && o.focusTab > 0) {
            for (int k = 0; k < o.focusTab; k++) ui.Key(VK_TAB, true, false);
        }
        hr = RenderFrame(rt.Get(), 0.6f);
        RunDeferred();
    }
    if (FAILED(hr)) return 4;
    gfx.Unbind();
    if (!o.demo) { g_net.Stop(); g_cams.Stop(); g_audio.Shutdown(); }
    return SavePng(bmp.Get(), o.out) ? 0 : 5;
}

static int RunDiag(const std::wstring& out, bool withList) {
    std::wstring r;
    r += Format(L"RoomHome %s (%s) diagnostics\r\n", APP_VERSION, APP_ARCH);
    r += L"Settings file: " + ConfigPath() + (FileExists(ConfigPath()) ? L" (exists)" : L" (not yet written, using defaults)") + L"\r\n";
    r += L"Fonts: " + gfx.famDisplay + L" / " + gfx.famText + L" / " + gfx.famIcon + L"\r\n";
    r += Format(L"Locale: 24-hour=%d, week starts Monday=%d; Windows apps theme light=%d\r\n", LocaleUses24h() ? 1 : 0,
                LocaleFirstDayIsMonday(), SystemPrefersLight() ? 1 : 0);
    r += Format(L"Autostart: user=%d machine=%d\r\n", AutostartUser() ? 1 : 0, AutostartMachine() ? 1 : 0);
    r += L"Computer: " + ComputerName() + L"\r\n";
    r += L"Edge: " + (FindEdge().empty() ? std::wstring(L"(not found)") : FindEdge()) + L"\r\n\r\nDock:\r\n";
    for (size_t i = 0; i < g.cfg.dock.size(); i++) {
        auto& it = g.cfg.dock[i];
        r += Format(L"  %zu. %s [%s] -> ", i + 1, it.name.c_str(), it.kind.c_str()) + DescribeLaunch(it) + L"\r\n";
        r += L"     icon source: " + IconSourceFor(it) + L"\r\n";
    }
    r += L"\r\nPresets:\r\n";
    for (int i = 0; i < kPresetCount; i++) {
        DockItem it = ItemFromPreset(kPresets[i]);
        r += L"  " + it.name + L" -> " + DescribeLaunch(it) + L"\r\n";
    }
    if (g_audio.Init(nullptr))
        r += Format(L"\r\nAudio: %s, level %.0f%%, muted=%d\r\n", g_audio.Device().c_str(), g_audio.Level() * 100.f, g_audio.Muted() ? 1 : 0);
    else
        r += L"\r\nAudio: no default playback device\r\n";
    g_audio.Shutdown();
    g_net.SetWantList(withList);
    g_net.Start(nullptr);
    g_net.WaitFirst(5000);
    if (withList) { Sleep(4500); }
    NetInfo n = g_net.Snapshot();
    g_net.Stop();
    r += Format(L"\r\nWi-Fi: service=%d present=%d radio=%d connected=%d connecting=%d denied=%d\r\n", n.wlanService, n.wifiPresent,
                n.wifiRadioOn, n.wifiConnected, n.wifiConnecting, n.wifiDenied);
    r += L"  adapter=" + n.wifiAdapter + L" ssid=" + n.wifiSsid + Format(L" quality=%d", n.wifiQuality) + L" ip=" + n.wifiIp +
         L" gw=" + n.wifiGateway + L" dns=" + n.wifiDns + L" mac=" + n.wifiMac + L"\r\n";
    if (withList) {
        r += Format(L"  networks valid=%d count=%zu\r\n", n.netsValid, n.nets.size());
        for (auto& w : n.nets)
            r += Format(L"    %-32s q=%3d secure=%d saved=%d connected=%d auth=%d cipher=%d supported=%d\r\n", w.ssid.c_str(),
                        w.quality, w.secure, w.hasProfile, w.connected, w.auth, w.cipher, w.supported);
    }
    r += Format(L"LAN: present=%d connected=%d speed=", n.lanPresent, n.lanConnected) + SpeedText(n.lanSpeed) + L" adapter=" +
         n.lanAdapter + L" ip=" + n.lanIp + L" gw=" + n.lanGateway + L" dns=" + n.lanDns + L" mac=" + n.lanMac + L"\r\n";
    r += Format(L"Internet: %d (local only %d)\r\n", n.internet, n.localOnly);

    // cameras: only what Windows reports about them; no camera is opened
    CamStatus cs = ScanCameras();
    r += Format(L"\r\nCameras: privacy-blocked=%d media-foundation-missing=%d count=%zu\r\n", cs.denied, cs.mfMissing, cs.cams.size());
    for (auto& d : cs.cams)
        r += Format(L"  %s [%s%s] problem=%d usable=%d id=%s\r\n    link=%s\r\n", d.name.c_str(),
                    d.external ? L"external" : L"built-in", d.virt ? L", virtual" : L"", d.problem, d.link.empty() ? 0 : 1,
                    d.hwid.c_str(), d.link.c_str());
    std::wstring missing;
    const CamDevice* room = RoomCamera(cs, &missing);
    r += L"  room camera: " + (room ? room->name : missing.empty() ? std::wstring(L"(none)") : missing + L" (not connected)") +
         (g.cfg.camera.empty() ? L" (not chosen yet)" : L"") + L"\r\n";
    std::string bytes = "\xEF\xBB\xBF" + WToUtf8(r);
    return WriteFileAtomic(out, bytes) ? 0 : 1;
}

// ---------------------------------------------------------------- entry
static bool ParseWH(const wchar_t* s, int& a, int& b, wchar_t sep) {
    const wchar_t* k = wcschr(s, sep);
    if (!k) return false;
    a = _wtoi(s);
    b = _wtoi(k + 1);
    return true;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
    QueryPerformanceFrequency(&s_freq);
    LangInit();   // before any worker thread can call Tr()
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring configPath, diagOut, machineAuto, openSheet, langArg;
    bool quit = false, diagList = false;
    ShotOpts shot;
    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        auto next = [&]() -> std::wstring { return i + 1 < argc ? std::wstring(argv[++i]) : std::wstring(); };
        if (a == L"--quit") quit = true;
        else if (a == L"--screenshot") shot.out = next();
        else if (a == L"--size") { std::wstring v = next(); ParseWH(v.c_str(), shot.w, shot.h, L'x'); }
        else if (a == L"--sheet") shot.sheet = next();
        else if (a == L"--camstate") shot.camState = next();
        else if (a == L"--theme") shot.theme = next();
        else if (a == L"--edit") shot.edit = true;
        else if (a == L"--ambient") shot.ambient = true;
        else if (a == L"--demo") shot.demo = true;
        else if (a == L"--hover") { std::wstring v = next(); int x = 0, y = 0; if (ParseWH(v.c_str(), x, y, L',')) { shot.hoverX = (float)x; shot.hoverY = (float)y; } }
        else if (a == L"--tab") shot.focusTab = _wtoi(next().c_str());
        else if (a == L"--diag") diagOut = next();
        else if (a == L"--diag-list") diagList = true;
        else if (a == L"--config") configPath = next();
        else if (a == L"--window") {
            std::wstring v = next();
            int w = 1280, h = 720;
            ParseWH(v.c_str(), w, h, L'x');
            g.windowed = true;
            s_windowRect = { 60, 60, 60 + w, 60 + h };
        } else if (a == L"--pos") {
            std::wstring v = next();
            int x = 0, y = 0;
            if (ParseWH(v.c_str(), x, y, L',')) {
                LONG w = s_windowRect.right - s_windowRect.left, h = s_windowRect.bottom - s_windowRect.top;
                s_windowRect = { x, y, x + w, y + h };
            }
        } else if (a == L"--noactivate") s_noActivate = true;
        else if (a == L"--dry-run") g.dryRun = true;
        else if (a == L"--machine-autostart") machineAuto = next();
        else if (a == L"--open") openSheet = next();
        else if (a == L"--lang") langArg = next();
        else if (a == L"--log") { std::wstring p = next(); s_log = _wfsopen(p.c_str(), L"w", _SH_DENYNO); }   // tests read it live
    }
    LocalFree(argv);

    if (!machineAuto.empty()) return SetAutostartMachine(machineAuto == L"on") ? 0 : 1;

    if (quit) {
        HWND h = FindWindowW(APP_CLASS, nullptr);
        if (!h) return 0;
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        HANDLE ph = OpenProcess(SYNCHRONIZE, FALSE, pid);
        PostMessageW(h, WM_APP_QUIT, 0, 0);
        if (ph) { WaitForSingleObject(ph, 8000); CloseHandle(ph); }
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    g_lang = LangDetect(0);
    if (!ConfigLoad(g.cfg, configPath) && !configPath.empty()) {
        MessageBoxW(nullptr, Format(Tr(L"Couldn't read %s"), configPath.c_str()).c_str(), APP_NAME, MB_ICONERROR);
        return 1;
    }
    if (langArg == L"en") g.cfg.language = 1;
    if (langArg == L"th") g.cfg.language = 2;
    g_lang = LangDetect(g.cfg.language);
    g.systemLight = SystemPrefersLight();
    if (!gfx.Init()) {
        MessageBoxW(nullptr, Tr(L"Direct2D could not be started on this PC."), APP_NAME, MB_ICONERROR);
        return 1;
    }
    if (!diagOut.empty()) return RunDiag(diagOut, diagList);
    if (!shot.out.empty()) return RunScreenshot(shot);

    HANDLE mutex = nullptr;
    if (!g.windowed) {
        mutex = CreateMutexW(nullptr, TRUE, L"Local\\RoomHome.Instance");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            HWND h = FindWindowW(APP_CLASS, nullptr);
            if (h) {
                if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
                SetForegroundWindow(h);
            }
            return 0;
        }
    }

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));
    wc.hIconSm = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON), 0);
    wc.lpszClassName = APP_CLASS;
    RegisterClassExW(&wc);

    RECT r = TargetRect();
    DWORD ex = WS_EX_APPWINDOW | (s_noActivate ? WS_EX_NOACTIVATE : 0);
    g.hwnd = CreateWindowExW(ex, APP_CLASS, L"RoomHome", WS_POPUP | WS_CLIPCHILDREN, r.left, r.top, r.right - r.left,
                             r.bottom - r.top, nullptr, nullptr, hi, nullptr);
    if (!g.hwnd) return 1;
    edits.parent = g.hwnd;
    UpdateLayout((float)(r.right - r.left), (float)(r.bottom - r.top));

    g_net.Start(g.hwnd);
    g_audio.Init(g.hwnd);
    g_cams.Start(g.hwnd);
    // cameras coming and going (DBT_DEVNODES_CHANGED also reaches every top-level window)
    DEV_BROADCAST_DEVICEINTERFACE_W camFilter{};
    camFilter.dbcc_size = sizeof(camFilter);
    camFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    camFilter.dbcc_classguid = { 0xE5323777, 0xF976, 0x4F5B, { 0x9B, 0x55, 0xB9, 0x46, 0x99, 0xC4, 0x6E, 0x44 } };   // KSCATEGORY_VIDEO_CAMERA
    HDEVNOTIFY camNotify = RegisterDeviceNotificationW(g.hwnd, &camFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    RefreshAudio();
    RefreshAutostart();
    ApplyKeepAwake();
    s_lastFrame = Now();
    g.now = s_lastFrame;

    // Keep the window cloaked until the first frame is drawn, so it never flashes empty.
    BOOL cloak = TRUE;
    DwmSetWindowAttribute(g.hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
    ShowWindow(g.hwnd, s_noActivate ? SW_SHOWNOACTIVATE : SW_SHOW);
    RenderMain();
    cloak = FALSE;
    DwmSetWindowAttribute(g.hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
    if (!s_noActivate) SetForegroundWindow(g.hwnd);
    ScheduleClock();
    if (openSheet == L"rename") OpenSheet(Sheet::Rename);
    else if (openSheet == L"settings") OpenSheet(Sheet::Settings);
    else if (openSheet == L"wifi") OpenSheet(Sheet::Wifi);
    else if (openSheet == L"network") OpenSheet(Sheet::Network);
    else if (openSheet == L"power") OpenSheet(Sheet::Power);
    else if (openSheet == L"newprog") OpenEditor(-1, ItemFromPreset(kPresets[kPresetCount - 1]), true);
    else if (openSheet == L"addwidget") OpenSheet(Sheet::AddWidget);
    else if (openSheet == L"camera") {
        g_cams.WaitFirst(3000);   // the test needs to know the cameras first
        g.cams = g_cams.Snapshot();
        OnCamerasChanged();
        OpenSheet(Sheet::Camera);
    }
    else if (openSheet == L"edit") g.editMode = true;

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    if (camNotify) UnregisterDeviceNotification(camNotify);
    g_camPreview.Stop();
    g_cams.Stop();
    g_net.Stop();
    g_audio.Shutdown();
    CamWaitIdle(2000);   // a test picture that was still running releases its camera
    SetThreadExecutionState(ES_CONTINUOUS);
    gfx.Unbind();
    s_rt.Reset();
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    CoUninitialize();
    return 0;
}
