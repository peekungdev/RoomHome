// Home screen: background, status bar, clocks, calendar, dock, toasts, ambient mode.
#include "app.h"
#include "lang.h"


static int DayOfWeek(int y, int m, int d) {   // 0 = Sunday
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static int DaysIn(int y, int m) {
    static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[m - 1];
}

static bool Use24h() {
    static int cached = -1;
    if (cached < 0) cached = LocaleUses24h() ? 1 : 0;
    return g.cfg.clockMode == 1 || (g.cfg.clockMode == 0 && cached == 1);
}

static int WeekStart() {   // 0 Sunday, 1 Monday
    static int cached = -1;
    if (cached < 0) cached = LocaleFirstDayIsMonday();
    return g.cfg.weekStart == 1 ? 0 : g.cfg.weekStart == 2 ? 1 : cached;
}

// ======================================================================
// background
// ======================================================================
struct Orb { uint32_t col; float a, x, y, r, ax, ay, px, py; };
static const Orb kDarkOrbs[] = {
    { 0x3558FF, 0.52f, 0.17f, 0.26f, 0.64f, 0.06f, 0.05f, 610.f, 790.f },
    { 0x9B3BFF, 0.36f, 0.86f, 0.18f, 0.56f, 0.05f, 0.06f, 700.f, 530.f },
    { 0x00BFD0, 0.24f, 0.63f, 0.97f, 0.62f, 0.07f, 0.04f, 890.f, 640.f },
    { 0xFF4F8B, 0.17f, 0.08f, 0.98f, 0.46f, 0.04f, 0.05f, 480.f, 990.f },
};
static const Orb kLightOrbs[] = {
    { 0x7FB3FF, 0.55f, 0.15f, 0.22f, 0.64f, 0.06f, 0.05f, 610.f, 790.f },
    { 0xC9A6FF, 0.46f, 0.87f, 0.16f, 0.56f, 0.05f, 0.06f, 700.f, 530.f },
    { 0x7FE0D6, 0.44f, 0.63f, 0.98f, 0.62f, 0.07f, 0.04f, 890.f, 640.f },
    { 0xFFBEA1, 0.50f, 0.07f, 0.95f, 0.48f, 0.04f, 0.05f, 480.f, 990.f },
};

static Box CoverBox(D2D1_SIZE_F sz) {
    float k = std::max(g.vw / sz.width, g.vh / sz.height);
    float w = sz.width * k, h = sz.height * k;
    return { (g.vw - w) / 2, (g.vh - h) / 2, w, h };
}

static D2D1_COLOR_F WallTint(bool light) { return light ? Rgb(0xFFFFFF, 0.22f) : Rgb(0x000000, 0.34f); }

static ID2D1Bitmap* WallBitmap(WallEntry& w, const std::wstring& path) {
    if (path.empty()) return nullptr;
    if (w.path != path) { w = WallEntry(); w.path = path; }
    if (w.failed) return nullptr;
    if (!w.src) {
        w.src = gfx.LoadImageFile(path, (UINT)std::max(64.f, g.W), (UINT)std::max(64.f, g.H));
        if (!w.src) {
            w.failed = true;
            ShowToast(Format(Tr(L"Couldn't load the wallpaper %s"), path.c_str()), 2);
            return nullptr;
        }
    }
    if (!w.bmp || w.gen != gfx.gen) {
        w.bmp.Reset();
        w.blur.Reset();
        if (FAILED(gfx.mainRt->CreateBitmapFromWicBitmap(w.src.Get(), nullptr, &w.bmp))) { w.failed = true; return nullptr; }
        w.gen = gfx.gen;
    }
    return w.bmp.Get();
}

ID2D1Bitmap* WallpaperBitmap(bool light) {
    return WallBitmap(light ? g.wallLight : g.wallDark, light ? g.cfg.wallpaperLight : g.cfg.wallpaperDark);
}

static void BuildBlur(WallEntry& w, bool light) {
    if (!w.bmp || !gfx.dc) return;
    int bw = std::max(16, (int)(g.vw / 8)), bh = std::max(16, (int)(g.vh / 8));
    if (w.blur && w.blurW == bw && w.blurH == bh) return;
    w.blur.Reset();
    ComPtr<ID2D1BitmapRenderTarget> a, b;
    D2D1_SIZE_F sz = D2D1::SizeF((float)bw, (float)bh);
    D2D1_SIZE_U px = D2D1::SizeU(bw, bh);
    if (FAILED(gfx.mainRt->CreateCompatibleRenderTarget(sz, px, &a))) return;
    a->BeginDraw();
    a->Clear(D2D1::ColorF(0, 0, 0, 1));
    a->SetTransform(D2D1::Matrix3x2F::Scale(bw / g.vw, bh / g.vh));
    a->DrawBitmap(w.bmp.Get(), CoverBox(w.bmp->GetSize()).rect(), 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    a->FillRectangle(D2D1::RectF(0, 0, g.vw, g.vh), gfx.Solid(WallTint(light)));
    a->EndDraw();
    ComPtr<ID2D1Bitmap> secSz;
    a->GetBitmap(&secSz);
    if (FAILED(gfx.mainRt->CreateCompatibleRenderTarget(sz, px, &b))) return;
    ComPtr<ID2D1DeviceContext> bdc;
    if (FAILED(b.As(&bdc))) return;
    ComPtr<ID2D1Effect> fx;
    if (FAILED(bdc->CreateEffect(CLSID_D2D1GaussianBlur, &fx))) return;
    fx->SetInput(0, secSz.Get());
    fx->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, 3.5f);
    fx->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    bdc->BeginDraw();
    bdc->Clear(D2D1::ColorF(0, 0, 0, 1));
    bdc->DrawImage(fx.Get());
    bdc->EndDraw();
    b->GetBitmap(&w.blur);
    w.blurW = bw;
    w.blurH = bh;
}

ID2D1Brush* GlassBlurBrush() {
    static ComPtr<ID2D1BitmapBrush> br;
    static uint32_t brGen = 0;
    if (g.themeT > 0.001f && g.themeT < 0.999f) return nullptr;   // cross-fading
    bool light = g.themeT > 0.5f;
    WallEntry& w = light ? g.wallLight : g.wallDark;
    const std::wstring& path = light ? g.cfg.wallpaperLight : g.cfg.wallpaperDark;
    if (path.empty() || w.path != path || !w.blur) return nullptr;
    if (!br || brGen != gfx.gen) {
        br.Reset();
        if (FAILED(gfx.mainRt->CreateBitmapBrush(w.blur.Get(), &br))) return nullptr;
        brGen = gfx.gen;
    }
    br->SetBitmap(w.blur.Get());
    br->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    br->SetTransform(D2D1::Matrix3x2F::Scale(g.vw / w.blurW, g.vh / w.blurH));
    br->SetOpacity(1.f);
    return br.Get();
}

static void DrawAurora(bool light, float alpha) {
    Box all{ 0, 0, g.vw, g.vh };
    ID2D1LinearGradientBrush* lb = gfx.LinearV(all, light ? Rgb(0xEDF2FA) : Rgb(0x0B1230), light ? Rgb(0xF8F3EE) : Rgb(0x04050A));
    if (lb) { lb->SetOpacity(alpha); gfx.FillRectBrush(all, lb); }
    const Orb* orbs = light ? kLightOrbs : kDarkOrbs;
    double t = g.now;
    for (int i = 0; i < 4; i++) {
        const Orb& o = orbs[i];
        float cx = (o.x + o.ax * (float)std::sin(t * 2 * kPi / o.px)) * g.vw;
        float cy = (o.y + o.ay * (float)std::cos(t * 2 * kPi / o.py)) * g.vh;
        float ry = o.r * g.vh, rx = ry * 1.3f;
        ID2D1RadialGradientBrush* rb = gfx.Radial(cx, cy, rx, ry, Rgb(o.col, o.a));
        if (!rb) continue;
        rb->SetOpacity(alpha);
        gfx.FillRectBrush(Intersect({ cx - rx, cy - ry, 2 * rx, 2 * ry }, all), rb);
    }
}

static void DrawBgVariant(bool light, float alpha) {
    WallEntry& w = light ? g.wallLight : g.wallDark;
    const std::wstring& path = light ? g.cfg.wallpaperLight : g.cfg.wallpaperDark;
    if (ID2D1Bitmap* bmp = WallBitmap(w, path)) {
        gfx.rt->DrawBitmap(bmp, CoverBox(bmp->GetSize()).rect(), alpha, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        gfx.FillRect({ 0, 0, g.vw, g.vh }, MulA(WallTint(light), alpha));
        BuildBlur(w, light);
        return;
    }
    DrawAurora(light, alpha);
}

static void PaintBackground(bool light) {
    DrawBgVariant(light, 1.f);
    if (ID2D1BitmapBrush* nb = gfx.Noise()) {   // dither: hides banding in the big gradients
        nb->SetTransform(D2D1::Matrix3x2F::Scale(1.f / g.s, 1.f / g.s));
        nb->SetOpacity(0.022f);
        gfx.FillRectBrush({ 0, 0, g.vw, g.vh }, nb);
    }
}

// The background drifts by well under a pixel per second, so it is painted into a cached bitmap at
// most once a second, one cache per theme (both are drawn while the light/dark reveal runs).
// Animation frames - the second-hand tick, hover, sheets - then cost one bitmap copy instead of
// several full-screen fills.
void DrawBackground() {
    struct Cache {
        ComPtr<ID2D1BitmapRenderTarget> rt;
        uint32_t gen = 0;
        UINT w = 0, h = 0;
        double paintedAt = -100;
        std::wstring walls;
    };
    static Cache caches[2];
    bool light = g.themeT > 0.5f;
    Cache& c = caches[light ? 1 : 0];
    UINT pw = (UINT)std::ceil(g.W), ph = (UINT)std::ceil(g.H);
    std::wstring walls = light ? g.cfg.wallpaperLight : g.cfg.wallpaperDark;
    if (!c.rt || c.gen != gfx.gen || c.w != pw || c.h != ph) {
        c.rt.Reset();
        if (FAILED(gfx.mainRt->CreateCompatibleRenderTarget(D2D1::SizeF((float)pw, (float)ph), D2D1::SizeU(pw, ph), &c.rt))) {
            PaintBackground(light);
            return;
        }
        c.gen = gfx.gen;
        c.w = pw;
        c.h = ph;
        c.paintedAt = -100;
    }
    if (walls != c.walls || g.now - c.paintedAt >= 1.0 || g.now < c.paintedAt) {
        ID2D1RenderTarget* saved = gfx.rt;
        gfx.rt = c.rt.Get();
        c.rt->BeginDraw();
        c.rt->SetTransform(D2D1::Matrix3x2F::Scale(g.s, g.s));
        c.rt->Clear(D2D1::ColorF(0, 0, 0, 1));
        PaintBackground(light);
        c.rt->EndDraw();
        gfx.rt = saved;
        c.paintedAt = g.now;
        c.walls = walls;
    }
    ComPtr<ID2D1Bitmap> bmp;
    c.rt->GetBitmap(&bmp);
    D2D1_MATRIX_3X2_F world;
    gfx.rt->GetTransform(&world);
    gfx.rt->SetTransform(D2D1::Matrix3x2F::Identity());
    gfx.rt->DrawBitmap(bmp.Get(), D2D1::RectF(0, 0, (float)pw, (float)ph), 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    gfx.rt->SetTransform(world);
}

// ======================================================================
// status bar
// ======================================================================
std::wstring SpeedText(uint64_t bps) {
    if (bps >= 1000000000ULL) return Format(L"%.3g Gbps", bps / 1e9);
    if (bps >= 1000000ULL) return Format(L"%.0f Mbps", bps / 1e6);
    if (bps > 0) return Format(L"%.0f kbps", bps / 1e3);
    return L"Connected";
}

static int WifiLevel(int q) { return q >= 75 ? 4 : q >= 50 ? 3 : q >= 25 ? 2 : 1; }

static std::wstring PrimaryIp() {
    if (g.net.lanConnected && !g.net.lanIp.empty()) return g.net.lanIp;
    if (g.net.wifiConnected && !g.net.wifiIp.empty()) return g.net.wifiIp;
    return L"";
}

std::wstring RoomTitle() { return g.cfg.roomName.empty() ? Tr(std::wstring(L"Meeting Room")) : g.cfg.roomName; }

static void DrawTitle(float maxW) {
    const float M = 56.f;
    std::wstring title = RoomTitle();
    std::wstring sub = g.cfg.subtitle;
    if (sub.empty()) {
        std::wstring ip = PrimaryIp();
        sub = ip.empty() ? Tr(std::wstring(L"Offline")) : L"IP " + ip;
    }
    // names the user typed are shown as typed, even when they match an English label
    gfx.literal = true;
    float tw = std::min(gfx.TextWidth(title, FACE_DISPLAY, 40.f, W_SEMIBOLD), maxW - 70.f);
    float sw = std::min(gfx.TextWidth(sub, FACE_TEXT, 21.f, W_REGULAR), maxW - 70.f);
    gfx.literal = false;
    Box hit{ M - 18.f, 24.f, std::max(tw, sw) + 36.f + 52.f, 96.f };
    uint64_t id = Id("title");
    ui.Add(id, hit);
    float hov = ui.Anim(id ^ 0x51, ui.Highlighted(id) ? 1.f : 0.f, 14.f);
    if (hov > 0.01f) Glass(hit, 24.f, 0.4f, false, hov);
    gfx.literal = true;
    gfx.Text(title, { M, 28.f, tw + 4.f, 52.f }, FACE_DISPLAY, 40.f, W_SEMIBOLD, g.th.text);
    gfx.Text(sub, { M, 78.f, sw + 4.f, 32.f }, FACE_TEXT, 21.f, W_REGULAR, g.th.text2);
    gfx.literal = false;
    gfx.Glyph(0xE70F, { hit.r() - 58.f, hit.y + 26.f, 44.f, 44.f }, 22.f, MulA(g.th.text2, hov));
    FocusRing(id, hit, 24.f);
    if (ui.Clicked(id)) Defer([] { RequireUnlock([] { OpenSheet(Sheet::Rename); }); });
}

static void DrawVolume(Box b) {
    Glass(b, b.h / 2, 0, true);
    if (!g.audioOk) {
        gfx.Glyph(0xE74F, { b.x + 8.f, b.y, 56.f, b.h }, 24.f, g.th.text3);
        gfx.Text(L"No audio device", { b.x + 70.f, b.y, b.w - 90.f, b.h }, FACE_TEXT, 21.f, W_SEMIBOLD, g.th.text3);
        return;
    }
    float pad = 6.f, btn = b.h - 2 * pad;
    Box mute{ b.x + pad, b.y + pad, btn, btn };
    if (IconButton(Id("vol.mute"), mute, g.muted || g.volume <= 0.001f ? (wchar_t)0xE74F : (wchar_t)0xE767, BTN_PLAIN, 24.f))
        Defer([] { g_audio.ToggleMute(); g.muted = g_audio.Muted(); });
    Box minus{ mute.r() + 4.f, b.y + pad, btn, btn };
    // wide enough for "100%" and for "Mute" in the current language (Thai "ปิดเสียง" is wider), so
    // the bar does not jump when muting
    float pctW = std::max(64.f, std::max(gfx.TextWidth(L"100%", FACE_TEXT, 21.f, W_SEMIBOLD),
                                         gfx.TextWidth(L"Mute", FACE_TEXT, 21.f, W_SEMIBOLD)) + 12.f);
    Box pct{ b.r() - pad - pctW, b.y, pctW, b.h };
    Box plus{ pct.x - btn - 4.f, b.y + pad, btn, btn };
    Box bar{ minus.r() + 16.f, b.y, plus.x - 16.f - (minus.r() + 16.f), b.h };

    static double lastRepeat = 0;
    auto stepper = [&](uint64_t id, Box bx, wchar_t glyph, int dir) {
        bool clicked = IconButton(id, bx, glyph, BTN_PLAIN, 22.f);
        if (ui.Pressed(id) && ui.pointerDown && ui.now - ui.downTime > 0.45 && ui.now - lastRepeat > 0.12) {
            lastRepeat = ui.now;
            ui.activeConsumed = true;
            g_audio.Step(dir, g.cfg.volumeStep);
            g.volume = g_audio.Level();
            g.muted = g_audio.Muted();
        }
        if (clicked) Defer([dir] { g_audio.Step(dir, g.cfg.volumeStep); g.volume = g_audio.Level(); g.muted = g_audio.Muted(); });
    };
    stepper(Id("vol.minus"), minus, 0xE738, -1);
    stepper(Id("vol.plus"), plus, 0xE710, +1);

    uint64_t bid = Id("vol.bar");
    ui.Add(bid, bar, false);
    if (ui.active == bid && ui.pointerDown) {
        float v = Saturate((ui.px - bar.x) / bar.w);
        if (std::fabs(v - g.volume) > 0.004f) { g.volume = v; g_audio.SetLevel(v); g.muted = false; }
        ui.activeConsumed = true;
    }
    float lv = ui.Anim(Id("vol.level"), g.muted ? 0.f : g.volume, 20.f);
    Box track{ bar.x, bar.cy() - 4.f, bar.w, 8.f };
    gfx.FillRect(track, g.th.track, 4.f);
    gfx.FillRect({ track.x, track.y, track.w * lv, track.h }, g.muted ? g.th.text3 : g.th.accent, 4.f);
    float kx = track.x + track.w * lv;
    gfx.Shadow({ kx - 11.f, track.cy() - 11.f, 22.f, 22.f }, 11.f, 6.f, 0.25f, 2.f, Rgb(0x000000));
    gfx.FillCircle(kx, track.cy(), 11.f, Rgb(0xFFFFFF));
    gfx.Text(g.muted ? L"Mute" : Format(L"%d%%", (int)std::lround(g.volume * 100)), pct, FACE_TEXT, 21.f, W_SEMIBOLD,
             g.th.text2, AL_CENTER);
}

static void DrawNetPill(Box b, float wifiW) {
    NetInfo& n = g.net;
    Glass(b, b.h / 2, 0, true);
    Box ws{ b.x, b.y, wifiW, b.h }, ls{ b.x + wifiW, b.y, b.w - wifiW, b.h };
    uint64_t wid = Id("net.wifi"), lid = Id("net.lan");
    ui.Add(wid, ws);
    ui.Add(lid, ls);
    float hw = ui.Anim(wid ^ 0x51, ui.Highlighted(wid) ? 1.f : 0.f, 16.f);
    float hl = ui.Anim(lid ^ 0x51, ui.Highlighted(lid) ? 1.f : 0.f, 16.f);
    gfx.FillRect(ws.inset(5.f), MulA(g.th.glassHi, hw), (b.h - 10.f) / 2);
    gfx.FillRect(ls.inset(5.f), MulA(g.th.glassHi, hl), (b.h - 10.f) / 2);
    gfx.Line(ls.x, b.y + 16.f, ls.x, b.b() - 16.f, MulA(g.th.text3, 0.5f), 1.2f);

    // Wi-Fi
    std::wstring wt;
    D2D1_COLOR_F wc = g.th.text;
    int level = 0;
    if (!n.wifiPresent) { wt = L"No Wi-Fi"; wc = g.th.text3; }
    else if (!n.wifiRadioOn) { wt = L"Wi-Fi off"; wc = g.th.text3; }
    else if (n.wifiConnected) { wt = n.wifiSsid.empty() ? L"Wi-Fi" : n.wifiSsid; level = WifiLevel(n.wifiQuality); }
    else if (n.wifiConnecting) { wt = L"Connecting\x2026"; wc = g.th.text2; }
    else { wt = L"Not connected"; wc = g.th.text2; }
    DrawWifiIcon(ws.x + 40.f, ws.cy() - 2.f, 30.f, level, g.th.text, MulA(g.th.text3, 0.55f));
    gfx.Text(wt, { ws.x + 70.f, ws.y, ws.w - 88.f, ws.h }, FACE_TEXT, 21.f, W_SEMIBOLD, wc);
    FocusRing(wid, ws.inset(4.f), (b.h - 8.f) / 2);

    // LAN
    std::wstring lt;
    D2D1_COLOR_F lc = g.th.text;
    if (!n.lanPresent) { lt = L"No LAN"; lc = g.th.text3; }
    else if (n.lanConnected) lt = SpeedText(n.lanSpeed);
    else { lt = L"Unplugged"; lc = g.th.text2; }
    gfx.Glyph(0xE839, { ls.x + 14.f, ls.y, 44.f, ls.h }, 26.f, n.lanConnected ? g.th.text : g.th.text3);
    gfx.Text(lt, { ls.x + 64.f, ls.y, ls.w - 80.f, ls.h }, FACE_TEXT, 21.f, W_SEMIBOLD, lc);
    // internet dot
    bool any = n.lanConnected || n.wifiConnected;
    D2D1_COLOR_F dot = n.internet ? g.th.success : (any ? g.th.warn : g.th.danger);
    gfx.FillCircle(ls.x + 50.f, ls.cy() + 12.f, 6.5f, Mix(g.th.sheet, Rgb(0xFFFFFF), 0.f));
    gfx.FillCircle(ls.x + 50.f, ls.cy() + 12.f, 5.f, dot);
    FocusRing(lid, ls.inset(4.f), (b.h - 8.f) / 2);

    if (ui.Clicked(wid)) Defer([] { OpenSheet(Sheet::Wifi); });
    if (ui.Clicked(lid)) Defer([] { OpenSheet(Sheet::Network); });
}

static void DrawTopBar() {
    const float M = 56.f, y = 36.f, h = 66.f, gap = 14.f;
    float x = g.vw - M;
    Box power{ x - h, y, h, h };            x -= h + gap;
    Box settings{ x - h, y, h, h };         x -= h + gap;
    Box theme{ x - h, y, h, h };            x -= h + gap + 6.f;
    const float volW = 420.f;
    Box vol{ x - volW, y, volW, h };        x -= volW + gap + 6.f;

    NetInfo& n = g.net;
    std::wstring wt = n.wifiConnected && !n.wifiSsid.empty() ? n.wifiSsid : L"Not connected";
    float wifiW = Clamp(gfx.TextWidth(wt, FACE_TEXT, 21.f, W_SEMIBOLD) + 96.f, 200.f, 330.f);
    std::wstring lt = n.lanConnected ? SpeedText(n.lanSpeed) : L"Unplugged";
    float lanW = Clamp(gfx.TextWidth(lt, FACE_TEXT, 21.f, W_SEMIBOLD) + 96.f, 170.f, 230.f);
    float netW = wifiW + lanW;
    float titleMax = x - netW - M - 30.f;
    if (titleMax < 260.f) {   // narrow screen: squeeze the Wi-Fi name first
        wifiW = std::max(150.f, wifiW - (260.f - titleMax));
        netW = wifiW + lanW;
        titleMax = x - netW - M - 30.f;
    }
    Box net{ x - netW, y, netW, h };

    DrawTitle(std::max(200.f, titleMax));
    DrawNetPill(net, wifiW);
    DrawVolume(vol);
    bool light = g.themeT > 0.5f;
    g.themeOriginX = theme.cx();   // the light/dark reveal grows from this button
    g.themeOriginY = theme.cy();
    if (IconButton(Id("btn.theme"), theme, light ? (wchar_t)0xE706 : (wchar_t)0xE708, BTN_GLASS, 26.f))
        Defer([light] { SetTheme(light ? 1 : 2); });
    if (IconButton(Id("btn.settings"), settings, 0xE713, BTN_GLASS, 26.f))
        Defer([] { RequireUnlock([] { OpenSheet(Sheet::Settings); }); });
    if (IconButton(Id("btn.power"), power, 0xE7E8, BTN_DANGER, 26.f)) Defer([] { OpenSheet(Sheet::Power); });
}

// ======================================================================
// widgets: analog clock, digital clock, calendar, world clock, note
// ======================================================================
static int64_t DaysFromCivil(int y, int m, int d) {   // days since 1970-01-01
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static void CivilFromDays(int64_t z, int& y, int& m, int& d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    y = (int)(yoe + era * 400 + (m <= 2));
}

static int IsoWeek(int64_t dn) {
    int wd = (int)(((dn + 3) % 7 + 7) % 7) + 1;   // 1 = Monday (1970-01-01 was a Thursday)
    int64_t thu = dn + (4 - wd);
    int y, m, d;
    CivilFromDays(thu, y, m, d);
    return (int)((thu - DaysFromCivil(y, 1, 1)) / 7) + 1;
}

static void Hand(float cx, float cy, float angleDeg, float len, float tail, float w, D2D1_COLOR_F c, float shadowA) {
    float r = (angleDeg - 90.f) * kPi / 180.f;
    float dx = std::cos(r), dy = std::sin(r);
    if (shadowA > 0)
        gfx.Line(cx - dx * tail + 2.f, cy - dy * tail + 5.f, cx + dx * len + 2.f, cy + dy * len + 5.f, Rgb(0x000000, shadowA), w);
    gfx.Line(cx - dx * tail, cy - dy * tail, cx + dx * len, cy + dy * len, c, w);
}

static void DrawAnalogFace(float cx, float cy, float R, const SYSTEMTIME& t, bool secHand, bool numbers, bool dateTag) {
    Box face{ cx - R, cy - R, 2 * R, 2 * R };
    float light = g.th.light;
    gfx.Shadow(face, R, 46.f, Lerp(0.42f, 0.14f, light), 16.f, g.th.shadow);
    if (ID2D1Brush* bb = GlassBlurBrush()) gfx.FillRectBrush(face, bb, R);
    gfx.FillRect(face, Mix(Rgb(0xFFFFFF, 0.07f), Rgb(0xFFFFFF, 0.60f), light), R);
    if (ID2D1RadialGradientBrush* hb = gfx.Radial(cx - R * 0.35f, cy - R * 0.45f, R * 1.1f, R * 1.1f, Rgb(0xFFFFFF, Lerp(0.10f, 0.35f, light))))
        gfx.FillRectBrush(face, hb, R);
    gfx.StrokeCircle(cx, cy, R - 1.f, g.th.glassStroke, 2.f);

    for (int i = 0; i < 60; i++) {
        float a = (i * 6.f - 90.f) * kPi / 180.f;
        float c = std::cos(a), s = std::sin(a);
        if (i % 5 == 0) gfx.Line(cx + c * R * 0.83f, cy + s * R * 0.83f, cx + c * R * 0.92f, cy + s * R * 0.92f, g.th.text, R * 0.026f);
        else gfx.Line(cx + c * R * 0.885f, cy + s * R * 0.885f, cx + c * R * 0.92f, cy + s * R * 0.92f, g.th.text3, R * 0.009f);
    }
    if (numbers) {
        float ns = R * 0.14f;
        for (int hnum = 1; hnum <= 12; hnum++) {
            float a = (hnum * 30.f - 90.f) * kPi / 180.f;
            float px = cx + std::cos(a) * R * 0.68f, py = cy + std::sin(a) * R * 0.68f;
            gfx.Text(std::to_wstring(hnum), { px - ns, py - ns * 0.75f, ns * 2, ns * 1.5f }, FACE_DISPLAY, ns, W_SEMIBOLD, g.th.text2, AL_CENTER);
        }
    }
    if (dateTag) {
        std::wstring tag = DateTag(t);
        gfx.Text(tag, { cx - R * 0.5f, cy + R * 0.30f, R, R * 0.14f }, FACE_TEXT, R * 0.085f, W_BOLD, g.th.accent, AL_CENTER);
    }
    float ms = (float)t.wMilliseconds;
    float sec = (float)t.wSecond;
    float minA = (t.wMinute + sec / 60.f) * 6.f;
    float hrA = ((t.wHour % 12) + t.wMinute / 60.f) * 30.f;
    float sh = Lerp(0.35f, 0.18f, light);
    Hand(cx, cy, hrA, R * 0.50f, R * 0.07f, R * 0.055f, g.th.text, sh);
    Hand(cx, cy, minA, R * 0.78f, R * 0.07f, R * 0.036f, g.th.text, sh);
    if (secHand) {
        float tick = ms < 260.f ? EaseOutBack(ms / 260.f) : 1.f;
        if (ms < 275.f) ui.KeepAnimating("tick");
        Hand(cx, cy, (sec - 1.f + tick) * 6.f, R * 0.86f, R * 0.18f, R * 0.014f, g.th.accent, sh * 0.6f);
    }
    gfx.FillCircle(cx, cy, R * 0.045f, secHand ? g.th.accent : g.th.text);
    gfx.FillCircle(cx, cy, R * 0.018f, Mix(Rgb(0x0B1020), Rgb(0xFFFFFF), light));
}

static void DrawDigital(Box col, const WidgetCfg& w) {
    bool h24 = Use24h();
    int hr = g.st.wHour;
    std::wstring ampm;
    if (!h24) { ampm = hr < 12 ? L"AM" : L"PM"; hr %= 12; if (hr == 0) hr = 12; }
    std::wstring hm = h24 ? Format(L"%02d:%02d", hr, g.st.wMinute) : Format(L"%d:%02d", hr, g.st.wMinute);
    const float big = w.large ? 244.f : 196.f, secSz = w.large ? 104.f : 84.f;
    float hmW = gfx.TextWidth(hm, FACE_DISPLAY, big, W_LIGHT);
    float secW = w.seconds ? gfx.TextWidth(L"00", FACE_DISPLAY, secSz, W_REGULAR) : 0.f;
    float gapS = w.seconds ? 18.f : 0.f;
    float ampmW = (!ampm.empty() && !w.seconds) ? gfx.TextWidth(ampm, FACE_TEXT, 30.f, W_SEMIBOLD) + 14.f : 0.f;
    float total = hmW + gapS + secW + ampmW;
    float x = col.cx() - total / 2;
    float lineH = big * 1.32f;
    float blockH = (w.greeting ? 40.f : 0.f) + lineH + (w.date ? 42.f : -8.f);
    float top = col.cy() - blockH / 2;

    if (w.greeting) {
        int h = g.st.wHour;
        const wchar_t* greet = h < 5 ? L"Good evening" : h < 12 ? L"Good morning" : h < 18 ? L"Good afternoon" : L"Good evening";
        gfx.Text(greet, { col.x, top, col.w, 44.f }, FACE_TEXT, w.large ? 34.f : 30.f, W_SEMIBOLD, g.th.text2, AL_CENTER);
    }
    float lineTop = top + (w.greeting ? 40.f : 0.f);
    gfx.Text(hm, { x, lineTop, hmW + 6.f, lineH }, FACE_DISPLAY, big, W_LIGHT, g.th.text, AL_LEFT, false);
    float blBig = gfx.Baseline(FACE_DISPLAY, big, W_LIGHT);
    if (w.seconds) {
        float blSmall = gfx.Baseline(FACE_DISPLAY, secSz, W_REGULAR);
        Box sb{ x + hmW + gapS, lineTop + blBig - blSmall, secW + 6.f, secSz * 1.32f };
        // Only the digit that changes moves: the old one rises a little and fades, the new one
        // rises into place. No clip rectangle, so no digit is ever sliced in half.
        float t = (float)Saturate((float)((g.now - g.secChangedAt) / 0.32));
        if (t < 1.f) ui.KeepAnimating("secslide");
        float dw = gfx.TextWidth(L"0", FACE_DISPLAY, secSz, W_REGULAR);   // tabular figures: every digit this wide
        float lift = secSz * 0.20f;
        int cur = g.st.wSecond, prev = g.prevSec;
        for (int d = 0; d < 2; d++) {
            int cd = d == 0 ? cur / 10 : cur % 10;
            int pd = prev < 0 ? cd : (d == 0 ? prev / 10 : prev % 10);
            Box db{ sb.x + d * dw, sb.y, dw + 6.f, sb.h };
            std::wstring cs(1, (wchar_t)(L'0' + cd));
            if (pd == cd || t >= 1.f) {
                gfx.Text(cs, db, FACE_DISPLAY, secSz, W_REGULAR, g.th.accent, AL_LEFT, false);
                continue;
            }
            float out = EaseOutCubic(Saturate(t / 0.55f));
            float in = EaseOutCubic(Saturate((t - 0.2f) / 0.8f));
            gfx.Text(std::wstring(1, (wchar_t)(L'0' + pd)), db.moved(0, -out * lift), FACE_DISPLAY, secSz, W_REGULAR,
                     MulA(g.th.accent, 1.f - out), AL_LEFT, false);
            gfx.Text(cs, db.moved(0, (1.f - in) * lift), FACE_DISPLAY, secSz, W_REGULAR, MulA(g.th.accent, in), AL_LEFT, false);
        }
        if (!ampm.empty())
            gfx.Text(ampm, { sb.x, lineTop + blBig - big * 0.72f, 90.f, 40.f }, FACE_TEXT, 30.f, W_SEMIBOLD, g.th.text2, AL_LEFT, false);
    } else if (!ampm.empty()) {
        gfx.Text(ampm, { x + hmW + 14.f, lineTop + blBig - big * 0.72f, 90.f, 40.f }, FACE_TEXT, 30.f, W_SEMIBOLD, g.th.text2, AL_LEFT, false);
    }
    if (w.date) {
        std::wstring date = DateLong(g.st);
        gfx.Text(date, { col.x - 40.f, lineTop + lineH - 8.f, col.w + 80.f, 50.f }, FACE_TEXT, w.large ? 38.f : 34.f, W_REGULAR,
                 g.th.text2, AL_CENTER);
    }
}

static void DrawCalendar(Box b, const WidgetCfg& w) {
    Glass(b, 34.f, 0, true);
    int year = g.st.wYear, month = g.st.wMonth + g.calOffset;
    while (month < 1) { month += 12; year--; }
    while (month > 12) { month -= 12; year++; }

    uint64_t tid = Id("cal.title");
    Box titleBox{ b.x + 30.f, b.y + 24.f, b.w - 60.f - 124.f, 56.f };
    if (g.calOffset != 0) ui.Add(tid, titleBox);
    gfx.Text(MonthYear(year, month), titleBox, FACE_DISPLAY, 32.f, W_SEMIBOLD, g.th.text);
    if (g.calOffset != 0) {
        gfx.Text(L"Tap for today", { titleBox.x, titleBox.b() - 6.f, titleBox.w, 24.f }, FACE_TEXT, 17.f, W_SEMIBOLD, g.th.accent);
        if (ui.Clicked(tid)) g.calOffset = 0;
    }
    if (IconButton(Id("cal.prev"), { b.r() - 30.f - 116.f, b.y + 26.f, 54.f, 54.f }, 0xE76B, BTN_PLAIN, 22.f)) { g.calOffset--; g.calTouched = g.now; }
    if (IconButton(Id("cal.next"), { b.r() - 30.f - 54.f, b.y + 26.f, 54.f, 54.f }, 0xE76C, BTN_PLAIN, 22.f)) { g.calOffset++; g.calTouched = g.now; }

    int ws = WeekStart();
    float wkW = w.weekNumbers ? 52.f : 0.f;
    float gx = b.x + 22.f + wkW, gw = b.w - 44.f - wkW;
    float cellW = gw / 7.f;
    float wy = b.y + 100.f;
    if (w.weekNumbers) gfx.Text(L"WK", { b.x + 22.f, wy, wkW, 30.f }, FACE_TEXT, 16.f, W_BOLD, MulA(g.th.accent, 0.8f), AL_CENTER);
    for (int i = 0; i < 7; i++) {
        int dow = (ws + i) % 7;
        bool weekend = dow == 0 || dow == 6;
        gfx.Text(DayShort(dow), { gx + i * cellW, wy, cellW, 30.f }, FACE_TEXT, 16.f, W_BOLD,
                 weekend ? g.th.text3 : g.th.text2, AL_CENTER);
    }
    float gy = wy + 40.f;
    float rowH = (b.b() - 18.f - gy) / 6.f;
    int lead = (DayOfWeek(year, month, 1) - ws + 7) % 7;
    int dim = DaysIn(year, month);
    int pm = month == 1 ? 12 : month - 1, py = month == 1 ? year - 1 : year;
    int dimPrev = DaysIn(py, pm);
    bool thisMonth = g.calOffset == 0;
    int64_t first = DaysFromCivil(year, month, 1);
    for (int r = 0; r < 6 && w.weekNumbers; r++) {
        int monCell = r * 7 + (ws == 0 ? 1 : 0);           // the Monday of this row
        int wk = IsoWeek(first + (monCell - lead));
        gfx.Text(std::to_wstring(wk), { b.x + 22.f, gy + r * rowH, wkW, rowH }, FACE_TEXT, 17.f, W_SEMIBOLD,
                 MulA(g.th.accent, 0.8f), AL_CENTER);
    }
    for (int cell = 0; cell < 42; cell++) {
        int r = cell / 7, c = cell % 7;
        int day = cell - lead + 1;
        bool inMonth = day >= 1 && day <= dim;
        int shown = inMonth ? day : (day < 1 ? dimPrev + day : day - dim);
        float cx = gx + (c + 0.5f) * cellW, cy = gy + (r + 0.5f) * rowH;
        int dow = (ws + c) % 7;
        bool weekend = dow == 0 || dow == 6;
        bool today = thisMonth && inMonth && day == g.st.wDay;
        D2D1_COLOR_F col = !inMonth ? MulA(g.th.text3, 0.55f) : weekend ? g.th.text2 : g.th.text;
        if (today) {
            float rr = std::min(cellW, rowH) * 0.43f;
            gfx.Shadow({ cx - rr, cy - rr, 2 * rr, 2 * rr }, rr, 10.f, 0.25f, 3.f, Darken(g.th.accent, 0.5f));
            gfx.FillCircle(cx, cy, rr, g.th.accent);
            col = g.th.onAccent;
        }
        gfx.Text(std::to_wstring(shown), { cx - cellW / 2, cy - rowH / 2, cellW, rowH }, FACE_TEXT, 22.f,
                 today ? W_BOLD : W_REGULAR, col, AL_CENTER);
    }
}

static void DrawWorldClock(Box b, const WidgetCfg& w) {
    Glass(b, 34.f, 0, true);
    SYSTEMTIME zt{};
    int ahead = 0;
    bool ok = !w.zone.empty() && ZoneNow(w.zone, zt, &ahead);
    std::wstring city = Trim(w.city).empty() ? L"World clock" : w.city;
    gfx.Text(city, { b.x + 30.f, b.y + 22.f, b.w - 100.f, 46.f }, FACE_DISPLAY, 30.f, W_SEMIBOLD, g.th.text);
    if (!ok) {
        gfx.Text(L"Choose a time zone", { b.x + 30.f, b.y + 70.f, b.w - 60.f, 30.f }, FACE_TEXT, 20.f, W_REGULAR, g.th.text2);
        gfx.Glyph(0xE774, { b.x, b.y + 100.f, b.w, b.h - 120.f }, 80.f, g.th.text3);
        return;
    }
    bool day = zt.wHour >= 6 && zt.wHour < 18;
    gfx.Glyph(day ? 0xE706 : 0xE708, { b.r() - 76.f, b.y + 22.f, 48.f, 48.f }, 26.f, day ? g.th.warn : g.th.text2);
    SYSTEMTIME lt;
    GetLocalTime(&lt);
    int64_t zd = DaysFromCivil(zt.wYear, zt.wMonth, zt.wDay), ld = DaysFromCivil(lt.wYear, lt.wMonth, lt.wDay);
    std::wstring rel = Tr(zd == ld ? L"Today" : zd > ld ? L"Tomorrow" : L"Yesterday");
    std::wstring off;
    if (ahead == 0) off = Tr(L"same time as here");
    else {
        int a = std::abs(ahead);
        off = Format(Tr(L"%s%d h"), ahead > 0 ? L"+" : L"\x2212", a / 60);
        if (a % 60) off += Format(Tr(L" %d min"), a % 60);
    }
    gfx.Text(rel + L" \x00B7 " + off, { b.x + 30.f, b.y + 66.f, b.w - 60.f, 30.f }, FACE_TEXT, 20.f, W_REGULAR, g.th.text2);
    if (w.analog) {
        float R = std::min(b.w - 90.f, b.h - 150.f) / 2;
        DrawAnalogFace(b.cx(), b.y + 112.f + R + 6.f, R, zt, w.seconds, true, false);
        return;
    }
    bool h24 = Use24h();
    int hr = zt.wHour;
    std::wstring ampm;
    if (!h24) { ampm = hr < 12 ? L"AM" : L"PM"; hr %= 12; if (hr == 0) hr = 12; }
    std::wstring hm = h24 ? Format(L"%02d:%02d", hr, zt.wMinute) : Format(L"%d:%02d", hr, zt.wMinute);
    const float big = 104.f, small2 = 44.f;
    float hmW = gfx.TextWidth(hm, FACE_DISPLAY, big, W_LIGHT);
    std::wstring tail = w.seconds ? Format(L"%02d", zt.wSecond) : ampm;
    float tailW = tail.empty() ? 0.f : gfx.TextWidth(w.seconds ? std::wstring(L"00") : tail, FACE_DISPLAY, small2, W_REGULAR) + 10.f;
    float x = b.cx() - (hmW + tailW) / 2;
    float lineTop = b.y + 104.f + (b.h - 104.f - big * 1.32f) / 2 - 10.f;
    gfx.Text(hm, { x, lineTop, hmW + 6.f, big * 1.32f }, FACE_DISPLAY, big, W_LIGHT, g.th.text, AL_LEFT, false);
    if (!tail.empty()) {
        float bl = gfx.Baseline(FACE_DISPLAY, big, W_LIGHT) - gfx.Baseline(FACE_DISPLAY, small2, W_REGULAR);
        gfx.Text(tail, { x + hmW + 10.f, lineTop + bl, tailW + 20.f, small2 * 1.4f }, FACE_DISPLAY, small2, W_REGULAR,
                 w.seconds ? g.th.accent : g.th.text2, AL_LEFT, false);
    }
    std::wstring date = DateDayMonth(zt, false);
    gfx.Text(date, { b.x + 20.f, b.b() - 58.f, b.w - 40.f, 32.f }, FACE_TEXT, 20.f, W_SEMIBOLD, g.th.text3, AL_CENTER);
}

static void DrawNote(Box b, const WidgetCfg& w) {
    Glass(b, 34.f, 0, true);
    if (w.color) gfx.FillRect(b, Rgb(w.color, Lerp(0.26f, 0.40f, g.th.light)), 34.f);
    gfx.FillRect({ b.x + 30.f, b.y + 32.f, 6.f, 38.f }, w.color ? Rgb(w.color) : g.th.accent, 3.f);
    gfx.Text(Trim(w.title).empty() ? L"Note" : w.title, { b.x + 50.f, b.y + 26.f, b.w - 80.f, 50.f }, FACE_DISPLAY,
             w.large ? 38.f : 32.f, W_SEMIBOLD, g.th.text);
    Box body{ b.x + 34.f, b.y + 94.f, b.w - 68.f, b.h - 120.f };
    gfx.PushClip(body);
    gfx.Text(w.text, body, FACE_TEXT, w.large ? 31.f : 25.f, W_REGULAR, g.th.text, AL_LEFT, false, true);
    gfx.PopClip();
}

// ---- photo ----
static const std::vector<std::wstring>& FolderPhotos(const std::wstring& dir, bool shuffle) {
    struct Listing { double at = -1e9; std::vector<std::wstring> files; };
    static std::unordered_map<std::wstring, Listing> cache;
    Listing& l = cache[dir + (shuffle ? L"|s" : L"|n")];
    double age = g.now - l.at;
    if (age >= 0 && age < (l.files.empty() ? 20.0 : 300.0)) return l.files;   // re-scan every 5 min (empty: 20 s)
    l.at = g.now;
    l.files.clear();
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
            if (IsImageFile(fd.cFileName)) l.files.push_back(dir + L"\\" + fd.cFileName);
        } while (FindNextFileW(h, &fd) && l.files.size() < 5000);
        FindClose(h);
    }
    if (shuffle)
        std::sort(l.files.begin(), l.files.end(), [](const std::wstring& a, const std::wstring& b) {
            return Mix64(HashBytes(a.data(), a.size() * 2)) < Mix64(HashBytes(b.data(), b.size() * 2));
        });
    else
        std::sort(l.files.begin(), l.files.end(), [](const std::wstring& a, const std::wstring& b) {
            return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
        });
    return l.files;
}

static ID2D1Bitmap* PhotoBitmap(const std::wstring& path, int tw, int th, bool* loading) {
    std::wstring key = path + Format(L"|%dx%d", tw, th);
    PhotoEntry& e = g.photos[key];
    e.lastUsed = g.now;
    if (e.failed) return nullptr;
    if (e.px.empty()) {
        if (g.screenshot) {
            PhotoMsg m;
            LoadPhotoSync(path, tw, th, m);
            if (!m.ok) { e.failed = true; return nullptr; }
            e.w = m.w; e.h = m.h; e.px = std::move(m.px);
        } else {
            if (!e.loading && g.hwnd) { e.loading = true; LoadPhotoAsync(key, path, tw, th, g.hwnd); }
            if (loading) *loading = true;
            return nullptr;
        }
    }
    if (!e.bmp || e.gen != gfx.gen) {
        e.bmp = gfx.BitmapFromPixels(e.w, e.h, e.px.data());
        e.gen = gfx.gen;
    }
    return e.bmp.Get();
}

static void DrawPhotoBitmap(ID2D1Bitmap* bmp, Box b, bool fit, float opacity) {
    D2D1_SIZE_F sz = bmp->GetSize();
    float k = fit ? std::min(b.w / sz.width, b.h / sz.height) : std::max(b.w / sz.width, b.h / sz.height);
    Box ib{ b.cx() - sz.width * k / 2, b.cy() - sz.height * k / 2, sz.width * k, sz.height * k };
    ComPtr<ID2D1BitmapBrush> br;
    if (FAILED(gfx.rt->CreateBitmapBrush(bmp, D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP,
                                                                         D2D1_BITMAP_INTERPOLATION_MODE_LINEAR), &br)))
        return;
    ComPtr<ID2D1BitmapBrush1> br1;
    if (SUCCEEDED(br.As(&br1))) br1->SetInterpolationMode1(D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    br->SetTransform(D2D1::Matrix3x2F::Scale(k, k) * D2D1::Matrix3x2F::Translation(ib.x, ib.y));
    br->SetOpacity(opacity);
    gfx.FillRectBrush(fit ? ib : b, br.Get(), fit ? 16.f : 30.f);
}

static void DrawPhoto(Box b, const WidgetCfg& w) {
    gfx.Shadow(b, 30.f, 30.f, Lerp(0.36f, 0.14f, g.th.light), 12.f, g.th.shadow);
    gfx.FillRect(b, Mix(Rgb(0x111318, 0.55f), Rgb(0xFFFFFF, 0.55f), g.th.light), 30.f);
    std::vector<std::wstring> one;
    const std::vector<std::wstring>* list = &one;
    if (!Trim(w.path).empty()) {
        if (w.folder) list = &FolderPhotos(w.path, w.shuffle);
        else one.push_back(w.path);
    }
    auto placeholder = [&](const wchar_t* title, const wchar_t* sub) {
        gfx.Glyph(0xE91B, { b.x, b.cy() - 90.f, b.w, 100.f }, 64.f, g.th.text3);
        gfx.Text(title, { b.x + 20.f, b.cy() + 18.f, b.w - 40.f, 36.f }, FACE_TEXT, 23.f, W_SEMIBOLD, g.th.text2, AL_CENTER);
        if (sub) gfx.Text(sub, { b.x + 20.f, b.cy() + 54.f, b.w - 40.f, 30.f }, FACE_TEXT, 18.f, W_REGULAR, g.th.text3, AL_CENTER);
    };
    size_t n = list->size();
    if (n == 0) {
        placeholder(Trim(w.path).empty() ? L"Choose a photo" : w.folder ? L"No pictures in this folder" : L"Photo not found",
                    Trim(w.path).empty() ? L"Edit the widget to pick one" : nullptr);
    } else {
        // the slideshow position comes from the clock, so it needs no state and survives restarts
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        double secs = (double)((((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000ULL) / 1000.0;
        int interval = std::max(5, w.interval);
        int64_t slot = (n > 1) ? (int64_t)(secs / interval) : 0;
        size_t cur = (size_t)(slot % (int64_t)n);
        size_t next = (cur + 1) % n;
        // cross-fade timed from when this picture first showed (frames can arrive up to a second late)
        struct Show { size_t cur = SIZE_MAX, prev = SIZE_MAX; double at = 0; const void* list = nullptr; };
        static std::unordered_map<uint32_t, Show> shows;
        Show& sh = shows[w.uid];
        if (sh.list != list || sh.cur >= n || sh.prev >= n) { sh.prev = SIZE_MAX; }
        if (sh.cur != cur) { sh.prev = (sh.cur < n && sh.list == list) ? sh.cur : SIZE_MAX; sh.cur = cur; sh.at = g.now; }
        sh.list = list;
        size_t prev = sh.prev;
        float fade = prev == SIZE_MAX ? 1.f : Saturate((float)((g.now - sh.at) / 1.2));
        int tw = (int)std::ceil(b.w * g.s), th = (int)std::ceil(b.h * g.s);
        tw = (tw + 31) / 32 * 32;
        th = (th + 31) / 32 * 32;
        bool loading = false;
        ID2D1Bitmap* bc = PhotoBitmap((*list)[cur], tw, th, &loading);
        ID2D1Bitmap* bp = (fade < 1.f && prev != SIZE_MAX) ? PhotoBitmap((*list)[prev], tw, th, nullptr) : nullptr;
        if (n > 1) PhotoBitmap((*list)[next], tw, th, nullptr);   // decode the next one ahead of time
        if (fade < 1.f) ui.KeepAnimating("photofade");
        if (bp) DrawPhotoBitmap(bp, b, w.fit, 1.f);
        if (bc) DrawPhotoBitmap(bc, b, w.fit, bp ? EaseInOut(fade) : 1.f);
        else if (!bp) placeholder(loading ? L"Loading\x2026" : L"Can't open this picture", nullptr);
        if (!Trim(w.caption).empty()) {
            Box cap{ b.x, b.b() - 92.f, b.w, 92.f };
            gfx.FillRectBrush(cap, gfx.LinearV(cap, Rgb(0x000000, 0.f), Rgb(0x000000, 0.62f)), 30.f);
            gfx.Text(w.caption, { b.x + 28.f, b.b() - 58.f, b.w - 56.f, 40.f }, FACE_TEXT, 24.f, W_SEMIBOLD, Rgb(0xFFFFFF), AL_LEFT);
        }
    }
    gfx.StrokeRect(b.inset(0.6f), g.th.glassStroke, 29.4f, 1.2f);
}

WSize WidgetSize(const WidgetCfg& w) {
    if (w.type == L"photo") return w.shape == 1 ? WSize{ 440.f, 440.f } : w.shape == 2 ? WSize{ 360.f, 480.f } : WSize{ 560.f, 380.f };
    if (w.type == L"analog") return { 448.f, 448.f };
    if (w.type == L"digital") return w.large ? WSize{ 800.f, 470.f } : WSize{ 640.f, 400.f };
    if (w.type == L"calendar") return { w.weekNumbers ? 522.f : 470.f, 500.f };
    if (w.type == L"worldclock") return w.analog ? WSize{ 390.f, 460.f } : WSize{ 390.f, 330.f };
    if (w.type == L"note") return { 460.f, 400.f };
    return { 300.f, 300.f };
}

void DrawWidget(const WidgetCfg& w, Box b) {
    if (w.type == L"analog") DrawAnalogFace(b.cx(), b.cy(), std::min(b.w, b.h) / 2 - 10.f, g.st, w.seconds, w.numbers, w.date);
    else if (w.type == L"digital") DrawDigital(b, w);
    else if (w.type == L"calendar") DrawCalendar(b, w);
    else if (w.type == L"worldclock") DrawWorldClock(b, w);
    else if (w.type == L"note") DrawNote(b, w);
    else if (w.type == L"photo") DrawPhoto(b, w);
}

void DrawWidgetPreview(const WidgetCfg& w, Box area) {
    WSize s = WidgetSize(w);
    float k = std::min(1.f, std::min(area.w / s.w, area.h / s.h));
    D2D1_MATRIX_3X2_F world;
    gfx.rt->GetTransform(&world);
    gfx.rt->SetTransform(D2D1::Matrix3x2F::Scale(k, k, D2D1::Point2F(area.cx(), area.cy())) * world);
    bool saved = ui.suppress;
    ui.suppress = true;
    DrawWidget(w, { area.cx() - s.w / 2, area.cy() - s.h / 2, s.w, s.h });
    ui.suppress = saved;
    gfx.rt->SetTransform(world);
}

void EnterEditMode() {
    RequireUnlock([] { g.editMode = true; });
}

static void DrawWidgetRow() {
    auto& ws = g.cfg.widgets;
    int n = (int)ws.size();
    float edit = ui.Anim(Id("widgets.editT"), g.editMode ? 1.f : 0.f, 10.f);
    bool showAdd = g.editMode && n < kMaxWidgets;
    float top = 132.f, bottom = DockTop() - 26.f;
    if (g.editMode) bottom -= 30.f;   // room for the hint line above the dock
    float cy = (top + bottom) / 2, availW = g.vw - 2 * 56.f, availH = bottom - top;

    int from = -1;
    if (g.wdragUid) {
        for (int i = 0; i < n; i++) if (ws[i].uid == g.wdragUid) from = i;
        if (from < 0 || !g.editMode) g.wdragUid = 0;
    }
    if (g.wdragUid && !ui.pointerDown) {   // drop
        int to = std::clamp(g.wdragInsert, 0, n - 1);
        if (from >= 0 && to != from) {
            WidgetCfg w = ws[from];
            ws.erase(ws.begin() + from);
            ws.insert(ws.begin() + to, w);
            SaveConfig();
        }
        g.wdragUid = 0;
        from = -1;
    }
    std::vector<int> order;
    for (int i = 0; i < n; i++) if (i != from) order.push_back(i);
    if (from >= 0) order.insert(order.begin() + std::min((int)order.size(), g.wdragInsert), from);

    struct Slot { int idx; WSize s; };
    std::vector<Slot> slots;
    for (int i : order) slots.push_back({ i, WidgetSize(ws[i]) });
    if (showAdd) slots.push_back({ -1, { 250.f, 330.f } });
    int cnt = (int)slots.size();
    if (cnt == 0) return;
    float sum = 0, maxH = 0;
    for (auto& s : slots) { sum += s.s.w; maxH = std::max(maxH, s.s.h); }
    const float minGap = 40.f;
    float k = std::min(1.f, std::min((availW - (cnt - 1) * minGap) / sum, availH / maxH));
    // edit mode moves the row up and may shrink it (room for "+ Add widget"): glide, don't jump
    k = ui.Anim(Id("wrow.k"), k, 12.f);
    cy = ui.Anim(Id("wrow.cy"), cy, 12.f);
    float gap = cnt > 1 ? std::min(170.f, (availW - sum * k) / (cnt - 1)) : 0.f;
    float total = sum * k + gap * (cnt - 1);
    std::vector<float> centers(cnt);
    float x = (g.vw - total) / 2;
    for (int s = 0; s < cnt; s++) { centers[s] = x + slots[s].s.w * k / 2; x += slots[s].s.w * k + gap; }

    if (g.wdragUid) {
        float dragCx = ui.px - g.wdragGrabX;
        int ins = 0;
        for (int s = 0; s < cnt; s++)
            if (slots[s].idx >= 0 && slots[s].idx != from && centers[s] < dragCx) ins++;
        g.wdragInsert = ins;
        ui.KeepAnimating("wdrag");
    }

    D2D1_MATRIX_3X2_F world;
    gfx.rt->GetTransform(&world);
    int dragIdx = -1;
    for (int s = 0; s < cnt; s++) {
        const Slot& sl = slots[s];
        if (sl.idx < 0) {   // "+ Add widget"
            float cx = ui.Anim(Id("w.addx"), centers[s], 16.f);
            Box nb{ cx - sl.s.w / 2, cy - sl.s.h / 2, sl.s.w, sl.s.h };
            gfx.rt->SetTransform(D2D1::Matrix3x2F::Scale(k, k, D2D1::Point2F(cx, cy)) * world);
            ui.SetScale(k, cx, cy);
            uint64_t id = Id("w.add");
            ui.Add(id, nb);
            float hov = ui.Anim(id ^ 0x51, ui.Highlighted(id) ? 1.f : 0.f, 14.f);
            float prs = ui.Anim(id ^ 0x52, ui.Pressed(id) ? 1.f : 0.f, 24.f);
            Box b = nb.scaled(1.f + 0.03f * hov - 0.04f * prs);
            gfx.FillRect(b, MulA(g.th.glassHi, (0.35f + 0.5f * hov) * edit), 30.f);
            gfx.StrokeRect(b.inset(1.f), MulA(g.th.text3, edit), 30.f, 2.f, true);
            gfx.Glyph(0xE710, { b.x, b.cy() - 70.f, b.w, 90.f }, 40.f, MulA(g.th.text2, edit));
            gfx.Text(L"Add widget", { b.x, b.cy() + 24.f, b.w, 40.f }, FACE_TEXT, 24.f, W_SEMIBOLD, MulA(g.th.text2, edit), AL_CENTER);
            ui.ClearScale();
            if (ui.Clicked(id)) Defer([] { OpenSheet(Sheet::AddWidget); });
            continue;
        }
        WidgetCfg& w = ws[sl.idx];
        if (w.uid == g.wdragUid) { dragIdx = sl.idx; continue; }
        float cx = ui.Anim(Id("w.x", w.uid), centers[s], 16.f);
        Box nb{ cx - sl.s.w / 2, cy - sl.s.h / 2, sl.s.w, sl.s.h };
        float jig = edit > 0.01f ? (float)std::sin(g.now * 13.0 + sl.idx * 2.3) * 0.6f * edit : 0.f;
        D2D1_POINT_2F c = D2D1::Point2F(cx, cy);
        gfx.rt->SetTransform(D2D1::Matrix3x2F::Rotation(jig, c) * D2D1::Matrix3x2F::Scale(k, k, c) * world);
        ui.SetScale(k, cx, cy);
        uint64_t bg = Id("w.bg", w.uid), cover = Id("w.cover", w.uid), badge = Id("w.badge", w.uid);
        if (!g.editMode) {
            ui.Add(bg, nb, false);   // under the widget's own buttons: press and hold to edit
            if (ui.LongPressed(bg, 0.7f) || ui.RightClicked(bg)) Defer([] { EnterEditMode(); });
        }
        Box outline = nb.inset(-14.f);
        if (edit > 0.01f) gfx.StrokeRect(outline, MulA(g.th.text3, 0.9f * edit), 30.f, 2.f, true);
        DrawWidget(w, nb);
        if (g.editMode) {
            ui.Add(cover, outline);   // on top: a tap edits the widget instead of using it
            float hov = ui.Anim(cover ^ 0x51, ui.Highlighted(cover) ? 1.f : 0.f, 14.f);
            if (hov > 0.01f) {
                gfx.FillRect(outline, MulA(g.th.glassHi, 0.30f * hov), 30.f);
                gfx.Text(L"Tap to edit", { outline.x, outline.b() - 46.f, outline.w, 34.f }, FACE_TEXT, 20.f, W_SEMIBOLD,
                         MulA(g.th.text, hov), AL_CENTER);
            }
            FocusRing(cover, outline, 30.f);
            Box bd{ outline.x - 20.f, outline.y - 20.f, 40.f, 40.f };
            ui.Add(badge, bd.inset(-8.f));
            float bh = ui.Anim(badge ^ 0x51, ui.Highlighted(badge) ? 1.f : 0.f, 16.f);
            gfx.Shadow(bd, 20.f, 6.f, 0.3f * edit, 2.f, Rgb(0x000000));
            gfx.FillCircle(bd.cx(), bd.cy(), 20.f + 2.f * bh, MulA(Lighten(g.th.danger, 0.1f * bh), edit));
            gfx.Line(bd.cx() - 8.f, bd.cy(), bd.cx() + 8.f, bd.cy(), MulA(Rgb(0xFFFFFF), edit), 3.5f);
            int idx = sl.idx;
            if (ui.Clicked(badge)) Defer([idx] { RemoveWidget(idx); });
            else if (ui.Clicked(cover)) Defer([idx] { OpenWidgetEditor(idx); });
            if (ui.active == cover && ui.pointerDown && ui.moved && (ui.dragOwner == 0 || ui.dragOwner == cover) && !g.wdragUid) {
                ui.dragOwner = cover;
                ui.activeConsumed = true;
                g.wdragUid = w.uid;
                g.wdragGrabX = ui.downX - cx;
                g.wdragInsert = s;
            }
            ui.KeepAnimating("jiggle");
        }
        ui.ClearScale();
    }
    if (dragIdx >= 0) {   // the dragged widget floats on top
        WidgetCfg& w = ws[dragIdx];
        WSize s = WidgetSize(w);
        float cx = ui.px - g.wdragGrabX;
        ui.AnimSet(Id("w.x", w.uid), cx);
        Box nb{ cx - s.w / 2, cy - s.h / 2, s.w, s.h };
        gfx.rt->SetTransform(D2D1::Matrix3x2F::Scale(k * 1.05f, k * 1.05f, D2D1::Point2F(cx, cy)) * world);
        gfx.StrokeRect(nb.inset(-14.f), g.th.accent, 30.f, 2.5f);
        bool saved = ui.suppress;
        ui.suppress = true;
        DrawWidget(w, nb);
        ui.suppress = saved;
    }
    gfx.rt->SetTransform(world);
}

// ======================================================================
// dock
// ======================================================================
struct DockGeo {
    Box shelf;
    float TW, TH, gap, pad, tileY;
    int slots;
};

static DockGeo ComputeDock() {
    DockGeo d{};
    int n = (int)g.cfg.dock.size();
    int cap = g.cfg.dockCapacity;
    d.gap = 30.f;
    d.pad = 30.f;
    const float extraRef = 150.f + 26.f;
    float avail = g.vw - 2 * 40.f - 2 * d.pad - extraRef;
    d.TW = std::min(232.f, (avail - (cap - 1) * d.gap) / cap);
    d.TH = std::round(d.TW * 0.6f);
    d.slots = g.editMode ? cap : std::min(cap, n + (n < cap ? 1 : 0));
    if (d.slots < 1) d.slots = 1;
    float extra = (g.editMode ? 150.f : 64.f) + 26.f;
    float labelH = 42.f;
    float shelfW = d.slots * d.TW + (d.slots - 1) * d.gap + 2 * d.pad + extra;
    float shelfH = 26.f + d.TH + 10.f + labelH + 16.f;
    d.shelf = { (g.vw - shelfW) / 2, g.vh - 30.f - shelfH, shelfW, shelfH };
    d.tileY = d.shelf.y + 26.f;
    return d;
}

float DockTop() { return ComputeDock().shelf.y; }

static ComPtr<ID2D1BitmapRenderTarget> s_tileRt[12];
static uint32_t s_tileGen[12];
static D2D1_SIZE_U s_tileSize[12];

static void DrawTileTilted(int slot, const DockItem& it, Box sb, float rx, float ry, float hl, float hxRel, float hyRel) {
    bool tilt = gfx.dc && slot >= 0 && slot < 12 && (std::fabs(rx) > 0.15f || std::fabs(ry) > 0.15f || hl > 0.02f);   // one path for the whole hover: no sharpness pop
    if (!tilt) { DrawTileFace(it, sb, hl, sb.x + hxRel * sb.w, sb.y + hyRel * sb.h); return; }
    float res = g.s;
    UINT pw = (UINT)std::ceil(sb.w * res) + 2, ph = (UINT)std::ceil(sb.h * res) + 2;
    if (!s_tileRt[slot] || s_tileGen[slot] != gfx.gen || s_tileSize[slot].width < pw || s_tileSize[slot].height < ph) {
        s_tileRt[slot].Reset();
        UINT aw = pw + 48, ah = ph + 48;
        if (FAILED(gfx.mainRt->CreateCompatibleRenderTarget(D2D1::SizeF((float)aw, (float)ah), D2D1::SizeU(aw, ah), &s_tileRt[slot]))) {
            DrawTileFace(it, sb, hl, sb.x + hxRel * sb.w, sb.y + hyRel * sb.h);
            return;
        }
        s_tileGen[slot] = gfx.gen;
        s_tileSize[slot] = D2D1::SizeU(aw, ah);
    }
    ID2D1BitmapRenderTarget* trt = s_tileRt[slot].Get();
    ID2D1RenderTarget* saved = gfx.rt;
    gfx.rt = trt;
    trt->BeginDraw();
    trt->Clear(D2D1::ColorF(0, 0, 0, 0));
    trt->SetTransform(D2D1::Matrix3x2F::Scale(res, res) * D2D1::Matrix3x2F::Translation(1.f, 1.f));
    DrawTileFace(it, { 0, 0, sb.w, sb.h }, hl, hxRel * sb.w, hyRel * sb.h);
    trt->EndDraw();
    gfx.rt = saved;
    ComPtr<ID2D1Bitmap> bmp;
    trt->GetBitmap(&bmp);
    D2D1_RECT_F src = D2D1::RectF(0, 0, (float)pw, (float)ph);
    Box dst{ sb.x - 1.f / res, sb.y - 1.f / res, pw / res, ph / res };
    float cx = sb.cx(), cy = sb.cy();
    D2D1_MATRIX_4X4_F m = D2D1::Matrix4x4F::Translation(-cx, -cy, 0) * D2D1::Matrix4x4F::RotationX(rx) *
                          D2D1::Matrix4x4F::RotationY(ry) * D2D1::Matrix4x4F::PerspectiveProjection(sb.w * 3.f) *
                          D2D1::Matrix4x4F::Translation(cx, cy, 0);
    gfx.dc->DrawBitmap(bmp.Get(), dst.rect(), 1.f, D2D1_INTERPOLATION_MODE_LINEAR, &src, &m);
}

static void DrawAddSlot(int slot, Box tb, float alpha) {
    uint64_t id = Id("dock.add", (uint64_t)slot);
    ui.Add(id, tb);
    float hov = ui.Anim(id ^ 0x51, ui.Highlighted(id) ? 1.f : 0.f, 14.f);
    float prs = ui.Anim(id ^ 0x52, ui.Pressed(id) ? 1.f : 0.f, 24.f);
    Box b = tb.scaled(1.f + hov * 0.05f - prs * 0.05f);
    float rad = b.h * 0.16f;
    gfx.FillRect(b, MulA(g.th.glassHi, (0.35f + 0.5f * hov) * alpha), rad);
    gfx.StrokeRect(b.inset(1.f), MulA(g.th.text3, alpha), rad, 2.f, true);
    gfx.Glyph(0xE710, b, b.h * 0.30f, MulA(g.th.text2, alpha));
    gfx.Text(L"Add app", { tb.x - 20.f, tb.b() + 10.f, tb.w + 40.f, 36.f }, FACE_TEXT, 22.f, W_SEMIBOLD,
             MulA(g.th.text3, alpha), AL_CENTER);
    if (ui.Clicked(id)) Defer([] { RequireUnlock([] { OpenSheet(Sheet::AddApp); }); });
}

static void DrawDock() {
    DockGeo d = ComputeDock();
    auto& items = g.cfg.dock;
    int n = (int)items.size();
    float shelfX = ui.Anim(Id("shelf.x"), d.shelf.x, 14.f);
    float shelfW = ui.Anim(Id("shelf.w"), d.shelf.w, 14.f);
    Box shelf{ shelfX, d.shelf.y, shelfW, d.shelf.h };
    Glass(shelf, 42.f, 0, true);
    float edit = ui.Anim(Id("dock.editT"), g.editMode ? 1.f : 0.f, 10.f);
    if (edit > 0.01f)
        gfx.Text(L"Drag to reorder \x00B7 tap a widget or app to change it \x00B7 \x2212 removes it", { 0, shelf.y - 52.f, g.vw, 40.f },
                 FACE_TEXT, 22.f, W_SEMIBOLD, MulA(g.th.text2, edit), AL_CENTER);

    auto slotX = [&](int i) { return d.shelf.x + d.pad + i * (d.TW + d.gap); };

    // drag state
    int from = -1;
    if (g.dragUid) {
        for (int i = 0; i < n; i++) if (items[i].uid == g.dragUid) from = i;
        if (from < 0 || !g.editMode) g.dragUid = 0;
    }
    if (g.dragUid && !ui.pointerDown) {   // drop
        int to = (int)Clamp((float)g.dragInsert, 0.f, (float)(n - 1));
        if (from >= 0 && to != from) {
            DockItem it = items[from];
            items.erase(items.begin() + from);
            items.insert(items.begin() + to, it);
            SaveConfig();
        }
        g.dragUid = 0;
        from = -1;
    }
    if (g.dragUid) {
        float left = ui.px - g.dragGrabX - (d.shelf.x + d.pad);
        g.dragInsert = (int)Clamp(std::round(left / (d.TW + d.gap)), 0.f, (float)(n - 1));
        ui.KeepAnimating("drag");
    }

    // visual order
    std::vector<int> order;
    for (int i = 0; i < n; i++) if (i != from) order.push_back(i);
    if (from >= 0) order.insert(order.begin() + std::min((int)order.size(), g.dragInsert), from);

    int dragSlot = -1;
    for (int slot = 0; slot < (int)order.size(); slot++) {
        int i = order[slot];
        DockItem& it = items[i];
        if (it.uid == g.dragUid) { dragSlot = slot; continue; }
        uint64_t id = Id("tile", it.uid);
        float tx = ui.Anim(Id("tile.x", it.uid), slotX(slot), 16.f);
        Box tb{ tx, d.tileY, d.TW, d.TH };
        ui.Add(id, tb);
        bool hlOn = ui.Highlighted(id) && !g.editMode;
        float f = ui.Anim(id ^ 0xA1, hlOn ? 1.f : 0.f, 13.f);
        float p = ui.Anim(id ^ 0xA2, ui.Pressed(id) ? 1.f : 0.f, 26.f);
        float pulse = ui.Anim(id ^ 0xA3, 0.f, 7.f);
        float txp = 0, typ = 0;
        if (ui.Hot(id) && !g.editMode) {
            txp = Clamp((ui.px - tb.cx()) / (tb.w / 2), -1.f, 1.f);
            typ = Clamp((ui.py - tb.cy()) / (tb.h / 2), -1.f, 1.f);
        }
        float rx = ui.Anim(id ^ 0xA4, -typ * 10.f * f, 10.f);
        float ry = ui.Anim(id ^ 0xA5, txp * 12.f * f, 10.f);
        float hx = ui.Anim(id ^ 0xA6, 0.5f + txp * 0.45f, 10.f), hy = ui.Anim(id ^ 0xA7, 0.25f + typ * 0.45f, 10.f);
        float scale = 1.f + 0.10f * f - 0.05f * p + 0.07f * pulse;
        Box sb = tb.scaled(scale).moved(0, -10.f * f);
        float rad = sb.h * 0.16f;
        gfx.Shadow(sb, rad, 16.f + 20.f * f, Lerp(0.34f, 0.13f, g.th.light) * (0.6f + 0.7f * f), 8.f + 12.f * f, g.th.shadow);

        D2D1_MATRIX_3X2_F world;
        if (edit > 0.01f) {
            float ang = (float)std::sin(g.now * 17.0 + i * 1.9) * 1.6f * edit;
            gfx.rt->GetTransform(&world);
            gfx.rt->SetTransform(D2D1::Matrix3x2F::Rotation(ang, D2D1::Point2F(sb.cx(), sb.cy())) * world);
            DrawTileFace(it, sb, 0, 0, 0);
            ui.KeepAnimating("jiggle");
        } else {
            DrawTileTilted(slot, it, sb, rx, ry, f * 0.9f, hx, hy);
        }
        gfx.Text(it.name, { tb.x - 18.f, tb.b() + 10.f + 8.f * f, tb.w + 36.f, 36.f }, FACE_TEXT, 22.f, W_SEMIBOLD,
                 Mix(g.th.text2, g.th.text, f), AL_CENTER);
        if (edit > 0.01f) {
            uint64_t bid = Id("tile.badge", it.uid);
            Box badge{ sb.x - 16.f, sb.y - 16.f, 40.f, 40.f };
            ui.Add(bid, badge.inset(-8.f));
            float bh = ui.Anim(bid ^ 0x51, ui.Highlighted(bid) ? 1.f : 0.f, 16.f);
            gfx.Shadow(badge, 20.f, 6.f, 0.3f * edit, 2.f, Rgb(0x000000));
            gfx.FillCircle(badge.cx(), badge.cy(), 20.f + 2.f * bh, MulA(Lighten(g.th.danger, 0.1f * bh), edit));
            gfx.Line(badge.cx() - 8.f, badge.cy(), badge.cx() + 8.f, badge.cy(), MulA(Rgb(0xFFFFFF), edit), 3.5f);
            gfx.rt->SetTransform(world);
            if (ui.Clicked(bid)) { int idx = i; Defer([idx] { RemoveDockItem(idx); }); }
        }

        if (!g.editMode) {
            if (ui.LongPressed(id, 0.7f) || ui.RightClicked(id))
                Defer([] { EnterEditMode(); });
            if (ui.Clicked(id)) {
                ui.AnimSet(id ^ 0xA3, 1.f);
                DockItem copy = it;
                Defer([copy] { LaunchItem(copy); });
            }
        } else {
            if (ui.active == id && ui.pointerDown && ui.moved && (ui.dragOwner == 0 || ui.dragOwner == id) && !g.dragUid) {
                ui.dragOwner = id;
                ui.activeConsumed = true;
                g.dragUid = it.uid;
                g.dragGrabX = ui.downX - tb.x;
                g.dragGrabY = ui.downY - tb.y;
                g.dragInsert = slot;
            }
            if (ui.Clicked(id)) { int idx = i; DockItem copy = it; Defer([idx, copy] { OpenEditor(idx, copy, false); }); }
        }
    }

    // empty slots
    int firstEmpty = (int)order.size();
    for (int s = firstEmpty; s < d.slots; s++) {
        Box tb{ slotX(s), d.tileY, d.TW, d.TH };
        DrawAddSlot(s, tb, g.editMode ? 1.f : 0.75f);
    }

    // the dragged tile floats on top
    if (dragSlot >= 0 && from >= 0) {
        DockItem& it = items[from];
        Box tb{ ui.px - g.dragGrabX, ui.py - g.dragGrabY, d.TW, d.TH };
        ui.AnimSet(Id("tile.x", it.uid), tb.x);
        Box sb = tb.scaled(1.08f);
        gfx.Shadow(sb, sb.h * 0.16f, 34.f, 0.40f, 18.f, g.th.shadow);
        DrawTileFace(it, sb, 0.3f, sb.cx(), sb.y);
    }

    // edit / done
    float bx = shelf.r() - d.pad;
    if (g.editMode) {
        Box done{ bx - 150.f, d.tileY + (d.TH - 62.f) / 2, 150.f, 62.f };
        if (Button(Id("dock.done"), done, L"Done", BTN_ACCENT, 0xE73E)) Defer([] { g.editMode = false; g.dragUid = 0; });
    } else {
        Box eb{ bx - 64.f, d.tileY + (d.TH - 64.f) / 2, 64.f, 64.f };
        if (IconButton(Id("dock.edit"), eb, 0xE70F, BTN_GLASS, 24.f))
            Defer([] { EnterEditMode(); });
    }
}

// ======================================================================
// composition
// ======================================================================
void DrawHome(float alpha) {
    if (alpha <= 0.01f) return;
    bool layer = alpha < 0.999f;
    if (layer) gfx.PushOpacity(alpha);
    DrawTopBar();
    DrawWidgetRow();
    DrawDock();
    if (layer) gfx.PopOpacity();
}

void DrawAmbient() {
    float a = Saturate(g.ambient * 2.f - 1.f);   // starts once the home screen has faded out
    if (a < 0.01f) return;
    gfx.FillRect({ -20.f, -20.f, g.vw + 40.f, g.vh + 40.f }, Rgb(0x000000, 0.74f * a));
    double t = g.now;
    float cx = g.vw * 0.5f + g.vw * 0.12f * (float)std::sin(t / 97.0);
    float cy = g.vh * 0.46f + g.vh * 0.10f * (float)std::sin(t / 71.0 + 1.0);
    bool h24 = Use24h();
    int hr = g.st.wHour;
    if (!h24) { hr %= 12; if (hr == 0) hr = 12; }
    std::wstring hm = h24 ? Format(L"%02d:%02d", hr, g.st.wMinute) : Format(L"%d:%02d", hr, g.st.wMinute);
    gfx.Text(hm, { cx - 500.f, cy - 160.f, 1000.f, 260.f }, FACE_DISPLAY, 210.f, W_LIGHT, Rgb(0xFFFFFF, 0.92f * a), AL_CENTER);
    std::wstring date = DateDayMonth(g.st, true);
    gfx.Text(date, { cx - 500.f, cy + 100.f, 1000.f, 50.f }, FACE_TEXT, 38.f, W_REGULAR, Rgb(0xFFFFFF, 0.70f * a), AL_CENTER);
    gfx.literal = true;
    gfx.Text(RoomTitle(), { cx - 500.f, cy + 156.f, 1000.f, 40.f }, FACE_TEXT, 26.f, W_SEMIBOLD, Rgb(0xFFFFFF, 0.45f * a), AL_CENTER);
    gfx.literal = false;
    gfx.Text(L"Touch the screen to start", { cx - 500.f, cy + 200.f, 1000.f, 34.f }, FACE_TEXT, 20.f, W_REGULAR,
             Rgb(0xFFFFFF, 0.35f * a), AL_CENTER);
}

void DrawToasts() {
    float y = 128.f;
    int saveLayer = ui.layer;
    ui.layer = g.sheet != Sheet::None ? 1 : 0;
    for (auto& t : g.toasts) {
        bool expired = g.now - t.born > t.life;
        float a = ui.Anim(Id("toast", t.id), expired ? 0.f : 1.f, 9.f);
        if (a < 0.01f) continue;
        float maxW = std::min(g.vw * 0.7f, 1100.f);
        float actW = t.actionLabel.empty() ? 0.f : gfx.TextWidth(t.actionLabel, FACE_TEXT, 21.f, W_SEMIBOLD) + 48.f;
        float textMax = maxW - 96.f - actW;
        IDWriteTextLayout* lay = gfx.Layout(t.text, FACE_TEXT, 22.f, W_SEMIBOLD, AL_LEFT, textMax, 200.f, true, false);
        DWRITE_TEXT_METRICS m{};
        if (lay) lay->GetMetrics(&m);
        float tw = std::min(textMax, m.widthIncludingTrailingWhitespace + 2.f);
        float th = std::max(30.f, m.height);
        float w = tw + 96.f + actW, h = th + 32.f;
        Box b{ (g.vw - w) / 2, y - (1.f - a) * 24.f, w, h };
        gfx.PushOpacity(a);
        gfx.Shadow(b, std::min(30.f, h / 2), 24.f, 0.30f, 10.f, g.th.shadow);
        gfx.FillRect(b, g.th.sheet, std::min(30.f, h / 2));
        gfx.StrokeRect(b.inset(0.6f), g.th.glassStroke, std::min(30.f, h / 2), 1.2f);
        wchar_t glyph = t.kind == 1 ? 0xE73E : t.kind == 2 ? 0xE7BA : 0xE946;
        D2D1_COLOR_F gc = t.kind == 1 ? g.th.success : t.kind == 2 ? g.th.danger : g.th.accent;
        gfx.Glyph(glyph, { b.x + 18.f, b.y, 40.f, b.h }, 24.f, gc);
        if (lay) gfx.rt->DrawTextLayout(D2D1::Point2F(b.x + 66.f, b.y + 16.f), lay, gfx.Solid(g.th.text));
        if (!t.actionLabel.empty()) {
            Box ab{ b.r() - actW - 10.f, b.cy() - 24.f, actW, 48.f };
            if (Button(Id("toast.act", t.id), ab, t.actionLabel, BTN_GLASS) && t.action) {
                auto act = t.action;
                t.life = 0;
                Defer(act);
            }
        }
        gfx.PopOpacity();
        y += (h + 12.f) * a;
    }
    ui.layer = saveLayer;
    g.toasts.erase(std::remove_if(g.toasts.begin(), g.toasts.end(),
                                  [](const Toast& t) {
                                      return g.now - t.born > t.life + 1.5;
                                  }),
                   g.toasts.end());
}

void DrawShutdown() {
    float a = ui.Anim(Id("shutdownT"), g.shuttingDown ? 1.f : 0.f, 8.f);
    if (a < 0.01f) return;
    gfx.FillRect({ -20.f, -20.f, g.vw + 40.f, g.vh + 40.f }, Rgb(0x000000, 0.86f * a));
    gfx.Text(g.shutdownText, { 0, g.vh / 2 - 60.f, g.vw, 80.f }, FACE_DISPLAY, 52.f, W_SEMIBOLD, Rgb(0xFFFFFF, a), AL_CENTER);
    float t = (float)std::fmod(g.now, 1.2) / 1.2f;
    gfx.Arc(g.vw / 2, g.vh / 2 + 80.f, 26.f, -90.f + t * 360.f, 130.f + t * 360.f, Rgb(0xFFFFFF, 0.8f * a), 4.f);
    ui.KeepAnimating("shutdown");
}
