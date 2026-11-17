// Theme + reusable controls drawn with Direct2D.
#include "app.h"

Theme MakeTheme(float t, uint32_t accent) {
    Theme d, l, r;
    d.text = Rgb(0xFFFFFF);            l.text = Rgb(0x15171C);
    d.text2 = Rgb(0xFFFFFF, 0.72f);    l.text2 = Rgb(0x15171C, 0.68f);
    d.text3 = Rgb(0xFFFFFF, 0.45f);    l.text3 = Rgb(0x15171C, 0.42f);
    d.glass = Rgb(0xFFFFFF, 0.085f);   l.glass = Rgb(0xFFFFFF, 0.50f);
    d.glassHi = Rgb(0xFFFFFF, 0.17f);  l.glassHi = Rgb(0xFFFFFF, 0.82f);
    d.glassStroke = Rgb(0xFFFFFF, 0.13f); l.glassStroke = Rgb(0xFFFFFF, 0.90f);
    d.sheet = Rgb(0x1A1D25, 0.97f);    l.sheet = Rgb(0xF4F5F8, 0.98f);
    d.field = Rgb(0x2B2F3A);           l.field = Rgb(0xFFFFFF);
    d.shadow = Rgb(0x000000);          l.shadow = Rgb(0x1B2340);
    d.dim = Rgb(0x02040A, 0.58f);      l.dim = Rgb(0x0E1424, 0.34f);
    d.track = Rgb(0xFFFFFF, 0.16f);    l.track = Rgb(0x15171C, 0.10f);
    d.tileGlass = Rgb(0x2C303B);       l.tileGlass = Rgb(0xFFFFFF);
    d.danger = Rgb(0xFF453A);          l.danger = Rgb(0xE5372C);
    d.success = Rgb(0x30D158);         l.success = Rgb(0x1FA846);
    d.warn = Rgb(0xFFD60A);            l.warn = Rgb(0xE59400);
    auto m = [t](D2D1_COLOR_F a, D2D1_COLOR_F b) { return Mix(a, b, t); };
    r.text = m(d.text, l.text); r.text2 = m(d.text2, l.text2); r.text3 = m(d.text3, l.text3);
    r.glass = m(d.glass, l.glass); r.glassHi = m(d.glassHi, l.glassHi); r.glassStroke = m(d.glassStroke, l.glassStroke);
    r.sheet = m(d.sheet, l.sheet); r.field = m(d.field, l.field); r.shadow = m(d.shadow, l.shadow); r.dim = m(d.dim, l.dim);
    r.track = m(d.track, l.track); r.tileGlass = m(d.tileGlass, l.tileGlass);
    r.danger = m(d.danger, l.danger); r.success = m(d.success, l.success); r.warn = m(d.warn, l.warn);
    r.accent = Rgb(accent);
    r.onAccent = Rgb(0xFFFFFF);
    r.light = t;
    return r;
}

void Glass(Box b, float radius, float hover, bool shadow, float alpha) {
    if (alpha <= 0.003f) return;
    if (shadow) gfx.Shadow(b, radius, 26.f, Lerp(0.30f, 0.11f, g.th.light) * alpha, 10.f, g.th.shadow);
    if (ID2D1Brush* bb = GlassBlurBrush()) {
        bb->SetOpacity(alpha);
        gfx.FillRectBrush(b, bb, radius);
    }
    gfx.FillRect(b, MulA(Mix(g.th.glass, g.th.glassHi, hover), alpha), radius);
    gfx.StrokeRect(b.inset(0.6f), MulA(g.th.glassStroke, alpha), std::max(0.f, radius - 0.6f), 1.2f);
}

void FocusRing(uint64_t id, Box b, float radius) {
    float f = ui.Anim(id ^ 0xF0C05ULL, ui.Focused(id) ? 1.f : 0.f, 18.f);
    if (f < 0.01f) return;
    gfx.StrokeRect(b.inset(-5.f), MulA(g.th.accent, f), radius + 5.f, 3.f);
}

static float LabelSize(float h) { return Clamp(h * 0.40f, 17.f, 28.f); }

bool Button(uint64_t id, Box b, const std::wstring& label, int style, wchar_t glyph, bool enabled) {
    if (enabled) ui.Add(id, b);
    float hov = ui.Anim(id ^ 0x51, enabled && ui.Highlighted(id) ? 1.f : 0.f, 16.f);
    float prs = ui.Anim(id ^ 0x52, ui.Pressed(id) ? 1.f : 0.f, 24.f);
    Box bb = b.scaled(1.f + hov * 0.025f - prs * 0.045f);
    float r = std::min(bb.h, bb.w) / 2;
    float a = enabled ? 1.f : 0.42f;
    D2D1_COLOR_F fg = g.th.text;
    switch (style) {
    case BTN_ACCENT:
        gfx.Shadow(bb, r, 14.f, 0.22f * a, 5.f, Darken(g.th.accent, 0.5f));
        gfx.FillRect(bb, MulA(Lighten(g.th.accent, hov * 0.12f), a), r);
        fg = g.th.onAccent;
        break;
    case BTN_DANGER:
        gfx.Shadow(bb, r, 14.f, 0.22f * a, 5.f, Darken(g.th.danger, 0.5f));
        gfx.FillRect(bb, MulA(Lighten(g.th.danger, hov * 0.12f), a), r);
        fg = Rgb(0xFFFFFF);
        break;
    case BTN_PLAIN:
        gfx.FillRect(bb, MulA(g.th.glassHi, hov * a), r);
        break;
    default:
        Glass(bb, r, hov, false, a);
        break;
    }
    float fs = LabelSize(bb.h);
    float gs = glyph ? fs * 1.1f : 0;
    float tw = label.empty() ? 0 : gfx.TextWidth(label, FACE_TEXT, fs, W_SEMIBOLD);
    float gap = (glyph && !label.empty()) ? 10.f : 0;
    float total = std::min(tw + gs + gap, bb.w - 24.f);
    float x = bb.cx() - total / 2;
    if (glyph) {
        gfx.Glyph(glyph, { x, bb.y, gs, bb.h }, gs, MulA(fg, a));
        x += gs + gap;
    }
    if (!label.empty())
        gfx.Text(label, { x, bb.y, std::max(10.f, total - gs - gap + 2), bb.h }, FACE_TEXT, fs, W_SEMIBOLD, MulA(fg, a), AL_LEFT);
    FocusRing(id, bb, r);
    return enabled && ui.Clicked(id);
}

bool IconButton(uint64_t id, Box b, wchar_t glyph, int style, float glyphSize, bool enabled) {
    if (enabled) ui.Add(id, b);
    float hov = ui.Anim(id ^ 0x51, enabled && ui.Highlighted(id) ? 1.f : 0.f, 16.f);
    float prs = ui.Anim(id ^ 0x52, ui.Pressed(id) ? 1.f : 0.f, 24.f);
    Box bb = b.scaled(1.f + hov * 0.06f - prs * 0.08f);
    float r = std::min(bb.w, bb.h) / 2;
    float a = enabled ? 1.f : 0.42f;
    D2D1_COLOR_F fg = g.th.text;
    if (style == BTN_ACCENT) { gfx.FillRect(bb, MulA(Lighten(g.th.accent, hov * 0.12f), a), r); fg = g.th.onAccent; }
    else if (style == BTN_DANGER) {
        Glass(bb, r, hov, true, a);
        gfx.FillRect(bb, MulA(g.th.danger, hov * 0.85f * a), r);
        fg = Mix(g.th.text, Rgb(0xFFFFFF), hov);
    } else if (style == BTN_PLAIN) gfx.FillRect(bb, MulA(g.th.glassHi, hov * a), r);
    else Glass(bb, r, hov, true, a);
    float gs = glyphSize > 0 ? glyphSize : bb.h * 0.42f;
    gfx.Glyph(glyph, bb, gs, MulA(fg, a));
    FocusRing(id, bb, r);
    return enabled && ui.Clicked(id);
}

bool Toggle(uint64_t id, Box b, bool on) {
    ui.Add(id, b);
    float t = ui.Anim(id ^ 0x77, on ? 1.f : 0.f, 18.f);
    float prs = ui.Anim(id ^ 0x78, ui.Pressed(id) ? 1.f : 0.f, 24.f);
    Box tr{ b.r() - 70.f, b.cy() - 20.f, 70.f, 40.f };
    gfx.FillRect(tr, Mix(g.th.track, g.th.accent, t), 20.f);
    float kw = 32.f + prs * 6.f;
    float kx = Lerp(tr.x + 4.f, tr.r() - 4.f - kw, t);
    Box knob{ kx, tr.y + 4.f, kw, 32.f };
    gfx.Shadow(knob, 16.f, 6.f, 0.25f, 2.f, Rgb(0x000000));
    gfx.FillRect(knob, Rgb(0xFFFFFF), 16.f);
    FocusRing(id, tr, 20.f);
    return ui.Clicked(id);
}

int Segmented(uint64_t id, Box b, const std::vector<std::wstring>& opts, int sel) {
    int n = (int)opts.size();
    if (n == 0) return sel;
    gfx.FillRect(b, g.th.track, b.h / 2);
    float w = b.w / n;
    float pos = ui.Anim(id ^ 0x99, (float)sel, 18.f);
    Box pill{ b.x + 3.f + pos * w, b.y + 3.f, w - 6.f, b.h - 6.f };
    gfx.Shadow(pill, pill.h / 2, 8.f, Lerp(0.20f, 0.12f, g.th.light), 2.f, g.th.shadow);
    gfx.FillRect(pill, Mix(Rgb(0xFFFFFF, 0.22f), Rgb(0xFFFFFF, 1.f), g.th.light), pill.h / 2);
    int result = sel;
    float fs = Clamp(b.h * 0.36f, 16.f, 24.f);
    for (int i = 0; i < n; i++) {
        Box seg{ b.x + i * w, b.y, w, b.h };
        uint64_t sid = Mix64(id + (uint64_t)i);
        ui.Add(sid, seg);
        float hov = ui.Anim(sid ^ 0x51, ui.Highlighted(sid) && i != sel ? 1.f : 0.f, 16.f);
        gfx.FillRect(seg.inset(3.f), MulA(g.th.glassHi, hov * 0.5f), (b.h - 6.f) / 2);
        gfx.Text(opts[i], seg.inset(6.f, 0), FACE_TEXT, fs, W_SEMIBOLD, i == sel ? g.th.text : g.th.text2, AL_CENTER);
        FocusRing(sid, seg.inset(2.f), (b.h - 4.f) / 2);
        if (ui.Clicked(sid)) result = i;
    }
    return result;
}

bool Swatch(uint64_t id, Box b, D2D1_COLOR_F c, bool selected) {
    ui.Add(id, b);
    float hov = ui.Anim(id ^ 0x31, ui.Highlighted(id) ? 1.f : 0.f, 16.f);
    float cx = b.cx(), cy = b.cy();
    float r = std::min(b.w, b.h) / 2 - 6.f + hov * 2.f;
    gfx.FillCircle(cx, cy, r, c);
    if (selected) {
        gfx.StrokeCircle(cx, cy, r + 4.5f, g.th.text, 2.5f);
        gfx.Glyph(0xE73E, { cx - r, cy - r, 2 * r, 2 * r }, r * 0.95f, Rgb(0xFFFFFF));
    }
    FocusRing(id, { cx - r, cy - r, 2 * r, 2 * r }, r);
    return ui.Clicked(id);
}

void Field(uint64_t id, Box b, std::wstring* value, const std::wstring& placeholder, int flags) {
    bool focused = edits.HasFocus(id);
    ui.Add(id, b);
    gfx.FillRect(b, g.th.field, 14.f);
    D2D1_COLOR_F border = focused ? g.th.accent : Mix(Rgb(0xFFFFFF, 0.14f), Rgb(0x15171C, 0.16f), g.th.light);
    gfx.StrokeRect(b.inset(0.75f), border, 13.f, focused ? 2.5f : 1.3f);
    bool multi = (flags & EF_MULTILINE) != 0;
    Box inner = multi ? b.inset(18.f, 14.f) : b.inset(18.f, 0);
    edits.Request(id, inner, value, placeholder, flags);
    if (!edits.IsShown() || g.screenshot) {
        std::wstring shown = value ? *value : L"";
        if ((flags & EF_PASSWORD) && !shown.empty()) shown = std::wstring(shown.size(), (wchar_t)0x25CF);
        gfx.PushClip(inner);
        if (shown.empty()) gfx.Text(placeholder, inner, FACE_TEXT, 24.f, W_REGULAR, g.th.text3, AL_LEFT, !multi, multi);
        else {
            // what is stored, never a translation of it (the edit box shows the same text)
            gfx.literal = true;
            gfx.Text(shown, inner, FACE_TEXT, 24.f, W_REGULAR, g.th.text, AL_LEFT, !multi, multi);
            gfx.literal = false;
        }
        gfx.PopClip();
    }
    FocusRing(id, b, 14.f);
    if (ui.Clicked(id)) Defer([id] { edits.Focus(id); });
}

void DrawWifiIcon(float cx, float cy, float size, int level, D2D1_COLOR_F on, D2D1_COLOR_F off) {
    float ox = cx, oy = cy + size * 0.34f;
    float w = std::max(1.5f, size * 0.105f);
    const float radii[3] = { size * 0.30f, size * 0.54f, size * 0.78f };
    for (int i = 0; i < 3; i++) gfx.Arc(ox, oy, radii[i], -135.f, -45.f, level > i + 1 ? on : off, w);
    gfx.FillCircle(ox, oy - size * 0.02f, size * 0.085f, level >= 1 ? on : off);
}

void DrawTileFace(const DockItem& it, Box b, float hl, float hx, float hy) {
    float rad = b.h * 0.16f;
    bool appicon = it.glyph == L"appicon";
    ID2D1Bitmap* ico = appicon ? IconFor(it) : nullptr;
    if (appicon) {
        D2D1_COLOR_F top = Mix(Rgb(0x3B404D), Rgb(0xFFFFFF), g.th.light);
        D2D1_COLOR_F bot = Mix(Rgb(0x22252E), Rgb(0xE6E9EF), g.th.light);
        gfx.FillRectBrush(b, gfx.LinearV(b, top, bot), rad);
    } else {
        D2D1_COLOR_F c = Rgb(it.color);
        gfx.FillRectBrush(b, gfx.LinearV(b, Lighten(c, 0.18f), Darken(c, 0.18f)), rad);
    }
    Box sheen{ b.x, b.y, b.w, b.h * 0.55f };
    gfx.FillRectBrush(b, gfx.LinearV(sheen, Rgb(0xFFFFFF, 0.20f), Rgb(0xFFFFFF, 0.f)), rad);
    if (ico) {
        D2D1_SIZE_F sz = ico->GetSize();
        float side = b.h * 0.60f;
        float k = side / std::max(sz.width, sz.height);
        Box ib{ b.cx() - sz.width * k / 2, b.cy() - sz.height * k / 2, sz.width * k, sz.height * k };
        ComPtr<ID2D1DeviceContext> d;
        if (SUCCEEDED(gfx.rt->QueryInterface(IID_PPV_ARGS(&d))))
            d->DrawBitmap(ico, ib.rect(), 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, nullptr, nullptr);
        else
            gfx.rt->DrawBitmap(ico, ib.rect(), 1.f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        wchar_t ch = appicon ? (wchar_t)0xECA5 : GlyphChar(it.glyph);
        gfx.Glyph(ch, b, b.h * 0.42f, appicon ? g.th.text : Rgb(0xFFFFFF));
    }
    if (hl > 0.01f) {
        ID2D1RadialGradientBrush* rb = gfx.Radial(hx, hy, b.w * 0.85f, b.w * 0.85f, Rgb(0xFFFFFF, 0.32f));
        if (rb) {
            rb->SetOpacity(Saturate(hl));
            gfx.FillRectBrush(b, rb, rad);
        }
    }
    gfx.StrokeRect(b.inset(0.75f), Rgb(0xFFFFFF, 0.16f + 0.10f * g.th.light), rad - 0.75f, 1.5f);
}
