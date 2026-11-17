#include "gfx.h"
#include "lang.h"

Gfx gfx;

static bool HasFamily(IDWriteFontCollection* c, const wchar_t* name) {
    UINT32 idx = 0;
    BOOL exists = FALSE;
    return c && SUCCEEDED(c->FindFamilyName(name, &idx, &exists)) && exists;
}

bool Gfx::Init() {
    D2D1_FACTORY_OPTIONS fo{};
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &fo,
                                 (void**)factory.GetAddressOf())))
        return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)dwrite.GetAddressOf())))
        return false;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))))
        return false;

    ComPtr<IDWriteFontCollection> sys;
    dwrite->GetSystemFontCollection(&sys);
    famDisplay = HasFamily(sys.Get(), L"Segoe UI Variable Display") ? L"Segoe UI Variable Display" : L"Segoe UI";
    famText = HasFamily(sys.Get(), L"Segoe UI Variable Text") ? L"Segoe UI Variable Text" : L"Segoe UI";
    famIcon = HasFamily(sys.Get(), L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
    famGdi = L"Segoe UI";

    if (SUCCEEDED(dwrite->CreateTypography(&tnum_))) {
        DWRITE_FONT_FEATURE f{ DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, 1 };
        tnum_->AddFontFeature(f);
    }
    D2D1_STROKE_STYLE_PROPERTIES sp = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                                  D2D1_LINE_JOIN_ROUND);
    factory->CreateStrokeStyle(sp, nullptr, 0, &roundCap);
    D2D1_STROKE_STYLE_PROPERTIES dp = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                                  D2D1_LINE_JOIN_ROUND, 10.f, D2D1_DASH_STYLE_CUSTOM);
    const float dashes[] = { 3.f, 3.f };
    factory->CreateStrokeStyle(dp, dashes, 2, &dashed);
    return true;
}

void Gfx::Bind(ID2D1RenderTarget* target) {
    Unbind();
    rt = mainRt = target;
    target->QueryInterface(IID_PPV_ARGS(&dc));
    target->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush);
    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    gen++;
}

void Gfx::Unbind() {
    linear_.clear();
    radial_.clear();
    noise_.Reset();
    layers_.clear();
    brush.Reset();
    dc.Reset();
    rt = mainRt = nullptr;
}

IDWriteTextFormat* Gfx::Format(Face f, float size, int weight, Align al, bool wrap, bool vcenter) {
    FmtKey k{ (int)f, (int)std::lround(size * 10), weight, (int)al, wrap ? 1 : 0, vcenter ? 1 : 0, g_lang };
    auto it = fmts_.find(k);
    if (it != fmts_.end()) return it->second.Get();
    const std::wstring& fam = f == FACE_DISPLAY ? famDisplay : f == FACE_TEXT ? famText : famIcon;
    ComPtr<IDWriteTextFormat> tf;
    // the locale drives line breaking: Thai has no spaces between words
    if (FAILED(dwrite->CreateTextFormat(fam.c_str(), nullptr, (DWRITE_FONT_WEIGHT)weight, DWRITE_FONT_STYLE_NORMAL,
                                        DWRITE_FONT_STRETCH_NORMAL, size, g_lang == LANG_TH ? L"th-th" : L"en-us", &tf)))
        return nullptr;
    tf->SetTextAlignment(al == AL_LEFT ? DWRITE_TEXT_ALIGNMENT_LEADING
                         : al == AL_CENTER ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_TRAILING);
    tf->SetParagraphAlignment(vcenter ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    tf->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    if (!wrap) {
        DWRITE_TRIMMING trim{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        ComPtr<IDWriteInlineObject> sign;
        dwrite->CreateEllipsisTrimmingSign(tf.Get(), &sign);
        tf->SetTrimming(&trim, sign.Get());
    }
    fmts_[k] = tf;
    return tf.Get();
}

IDWriteTextLayout* Gfx::Layout(const std::wstring& src, Face f, float size, int weight, Align al, float maxW, float maxH,
                               bool wrap, bool vcenter) {
    // Every static label passes through here, so this is where it is translated: what is
    // measured and what is drawn are always the same language.
    const std::wstring* tr = (f == FACE_ICON || literal) ? nullptr : TrFind(src);
    const std::wstring& s = tr ? *tr : src;
    struct { int face, size10, weight, al, w, h, wrap, vc, lang; } key{ (int)f, (int)std::lround(size * 10), weight, (int)al,
                                                                         (int)std::lround(maxW), (int)std::lround(maxH),
                                                                         wrap ? 1 : 0, vcenter ? 1 : 0, g_lang };
    uint64_t h = HashBytes(&key, sizeof(key));
    h = HashBytes(s.data(), s.size() * sizeof(wchar_t), h);
    auto it = layouts_.find(h);
    if (it != layouts_.end()) return it->second.Get();
    if (layouts_.size() > 1500) layouts_.clear();
    IDWriteTextFormat* tf = Format(f, size, weight, al, wrap, vcenter);
    if (!tf) return nullptr;
    ComPtr<IDWriteTextLayout> lay;
    if (FAILED(dwrite->CreateTextLayout(s.c_str(), (UINT32)s.size(), tf, std::max(maxW, 1.f), std::max(maxH, 1.f), &lay)))
        return nullptr;
    if (tnum_ && f != FACE_ICON) lay->SetTypography(tnum_.Get(), DWRITE_TEXT_RANGE{ 0, (UINT32)s.size() });
    layouts_[h] = lay;
    return lay.Get();
}

float Gfx::TextWidth(const std::wstring& s, Face f, float size, int weight) {
    IDWriteTextLayout* l = Layout(s, f, size, weight, AL_LEFT, 100000.f, size * 2, false, false);
    if (!l) return 0;
    DWRITE_TEXT_METRICS m{};
    l->GetMetrics(&m);
    return m.widthIncludingTrailingWhitespace;
}

float Gfx::Baseline(Face f, float size, int weight) {
    IDWriteTextLayout* l = Layout(L"0", f, size, weight, AL_LEFT, 1000.f, size * 2, false, false);
    if (!l) return size * 0.8f;
    DWRITE_LINE_METRICS lm{};
    UINT32 n = 0;
    l->GetLineMetrics(&lm, 1, &n);
    return lm.baseline;
}

void Gfx::Text(const std::wstring& s, Box b, Face f, float size, int weight, D2D1_COLOR_F c, Align al, bool vcenter, bool wrap) {
    if (s.empty() || c.a <= 0.003f) return;
    IDWriteTextLayout* l = Layout(s, f, size, weight, al, b.w, b.h, wrap, vcenter);
    if (!l) return;
    rt->DrawTextLayout(D2D1::Point2F(b.x, b.y), l, Solid(c), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}

void Gfx::Glyph(wchar_t ch, Box b, float size, D2D1_COLOR_F c) {
    if (!ch || c.a <= 0.003f) return;
    wchar_t s[2] = { ch, 0 };
    IDWriteTextLayout* l = Layout(s, FACE_ICON, size, W_REGULAR, AL_CENTER, b.w, b.h, false, true);
    if (l) rt->DrawTextLayout(D2D1::Point2F(b.x, b.y), l, Solid(c), D2D1_DRAW_TEXT_OPTIONS_NONE);
}

void Gfx::FillRect(Box b, D2D1_COLOR_F c, float radius) {
    if (c.a <= 0.002f || b.w <= 0 || b.h <= 0) return;
    if (radius > 0) rt->FillRoundedRectangle(D2D1::RoundedRect(b.rect(), radius, radius), Solid(c));
    else rt->FillRectangle(b.rect(), Solid(c));
}

void Gfx::FillRectBrush(Box b, ID2D1Brush* br, float radius) {
    if (!br || b.w <= 0 || b.h <= 0) return;
    if (radius > 0) rt->FillRoundedRectangle(D2D1::RoundedRect(b.rect(), radius, radius), br);
    else rt->FillRectangle(b.rect(), br);
}

void Gfx::StrokeRect(Box b, D2D1_COLOR_F c, float radius, float width, bool dash) {
    if (c.a <= 0.002f) return;
    ID2D1StrokeStyle* ss = dash ? dashed.Get() : nullptr;
    if (radius > 0) rt->DrawRoundedRectangle(D2D1::RoundedRect(b.rect(), radius, radius), Solid(c), width, ss);
    else rt->DrawRectangle(b.rect(), Solid(c), width, ss);
}

void Gfx::FillCircle(float cx, float cy, float r, D2D1_COLOR_F c) {
    if (c.a <= 0.002f) return;
    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), Solid(c));
}

void Gfx::StrokeCircle(float cx, float cy, float r, D2D1_COLOR_F c, float width) {
    if (c.a <= 0.002f) return;
    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), Solid(c), width);
}

void Gfx::Line(float x1, float y1, float x2, float y2, D2D1_COLOR_F c, float width) {
    if (c.a <= 0.002f) return;
    rt->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), Solid(c), width, roundCap.Get());
}

void Gfx::Arc(float cx, float cy, float r, float a0, float a1, D2D1_COLOR_F c, float width) {
    if (c.a <= 0.002f || std::fabs(a1 - a0) < 0.05f) return;
    if (a1 - a0 >= 359.9f) { StrokeCircle(cx, cy, r, c, width); return; }
    ComPtr<ID2D1PathGeometry> g;
    if (FAILED(factory->CreatePathGeometry(&g))) return;
    ComPtr<ID2D1GeometrySink> s;
    g->Open(&s);
    float r0 = a0 * kPi / 180.f, r1 = a1 * kPi / 180.f;
    s->BeginFigure(D2D1::Point2F(cx + r * std::cos(r0), cy + r * std::sin(r0)), D2D1_FIGURE_BEGIN_HOLLOW);
    s->AddArc(D2D1::ArcSegment(D2D1::Point2F(cx + r * std::cos(r1), cy + r * std::sin(r1)), D2D1::SizeF(r, r), 0,
                               a1 > a0 ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                               std::fabs(a1 - a0) > 180 ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
    s->EndFigure(D2D1_FIGURE_END_OPEN);
    s->Close();
    rt->DrawGeometry(g.Get(), Solid(c), width, roundCap.Get());
}

void Gfx::Shadow(Box b, float radius, float blur, float alpha, float dy, D2D1_COLOR_F col) {
    if (alpha <= 0.003f) return;
    const int n = 8;
    float each = 1.f - std::pow(1.f - Saturate(alpha), 1.f / n);
    for (int i = n; i >= 1; i--) {
        float grow = blur * ((float)i / n) - blur * 0.35f;
        Box s{ b.x - grow, b.y - grow + dy, b.w + 2 * grow, b.h + 2 * grow };
        if (s.w <= 0 || s.h <= 0) continue;
        FillRect(s, WithA(col, each), std::max(0.f, radius + grow));
    }
}

static uint64_t ColorKey(const D2D1_COLOR_F* c, int n, uint64_t salt) {
    return HashBytes(c, sizeof(D2D1_COLOR_F) * n, salt);
}

ID2D1LinearGradientBrush* Gfx::LinearV(Box b, D2D1_COLOR_F top, D2D1_COLOR_F bottom) {
    D2D1_COLOR_F cs[2] = { top, bottom };
    uint64_t k = ColorKey(cs, 2, 11);
    auto it = linear_.find(k);
    ID2D1LinearGradientBrush* br = nullptr;
    if (it != linear_.end()) br = it->second.Get();
    else {
        if (linear_.size() > 400) linear_.clear();
        D2D1_GRADIENT_STOP st[2] = { { 0.f, top }, { 1.f, bottom } };
        ComPtr<ID2D1GradientStopCollection> coll;
        ComPtr<ID2D1LinearGradientBrush> nb;
        if (FAILED(rt->CreateGradientStopCollection(st, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &coll))) return nullptr;
        if (FAILED(rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(), D2D1::Point2F(0, 1)),
                                                 coll.Get(), &nb)))
            return nullptr;
        linear_[k] = nb;
        br = nb.Get();
    }
    br->SetStartPoint(D2D1::Point2F(b.x, b.y));
    br->SetEndPoint(D2D1::Point2F(b.x, b.b()));
    br->SetOpacity(1.f);
    return br;
}

ID2D1RadialGradientBrush* Gfx::Radial(float cx, float cy, float rx, float ry, D2D1_COLOR_F c) {
    uint64_t k = ColorKey(&c, 1, 23);
    auto it = radial_.find(k);
    ID2D1RadialGradientBrush* br = nullptr;
    if (it != radial_.end()) br = it->second.Get();
    else {
        if (radial_.size() > 200) radial_.clear();
        // roughly Gaussian falloff
        D2D1_GRADIENT_STOP st[5] = { { 0.f, c }, { 0.3f, MulA(c, 0.78f) }, { 0.55f, MulA(c, 0.42f) },
                                     { 0.8f, MulA(c, 0.12f) }, { 1.f, WithA(c, 0) } };
        ComPtr<ID2D1GradientStopCollection> coll;
        ComPtr<ID2D1RadialGradientBrush> nb;
        if (FAILED(rt->CreateGradientStopCollection(st, 5, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &coll))) return nullptr;
        if (FAILED(rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(D2D1::Point2F(), D2D1::Point2F(), 1, 1),
                                                 coll.Get(), &nb)))
            return nullptr;
        radial_[k] = nb;
        br = nb.Get();
    }
    br->SetCenter(D2D1::Point2F(cx, cy));
    br->SetRadiusX(rx);
    br->SetRadiusY(ry);
    br->SetOpacity(1.f);
    return br;
}

ID2D1BitmapBrush* Gfx::Noise() {
    if (noise_) return noise_.Get();
    const int n = 128;
    std::vector<uint32_t> px(n * n);
    uint32_t s = 0x9E3779B9u;
    for (auto& p : px) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        uint32_t v = s & 255;
        uint32_t a = 255;
        // premultiplied grey
        p = (a << 24) | (v << 16) | (v << 8) | v;
    }
    ComPtr<ID2D1Bitmap> bmp = BitmapFromPixels(n, n, px.data());
    if (!bmp) return nullptr;
    D2D1_BITMAP_BRUSH_PROPERTIES bp = D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_WRAP, D2D1_EXTEND_MODE_WRAP,
                                                                  D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    rt->CreateBitmapBrush(bmp.Get(), bp, &noise_);
    return noise_.Get();
}

void Gfx::PushOpacity(float o) {
    o = Saturate(o);
    if ((int)layers_.size() <= layerDepth_) {
        ComPtr<ID2D1Layer> l;
        rt->CreateLayer(&l);
        layers_.push_back(l);
    }
    rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                        D2D1::IdentityMatrix(), o),
                  layers_[layerDepth_].Get());
    layerDepth_++;
}

void Gfx::PushMask(ID2D1Geometry* geo) {
    if ((int)layers_.size() <= layerDepth_) {
        ComPtr<ID2D1Layer> l;
        rt->CreateLayer(&l);
        layers_.push_back(l);
    }
    rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), geo, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE), layers_[layerDepth_].Get());
    layerDepth_++;
}

void Gfx::PopOpacity() {
    if (layerDepth_ <= 0) return;
    layerDepth_--;
    rt->PopLayer();
}

void Gfx::PushClip(Box b) { rt->PushAxisAlignedClip(b.rect(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE); }
void Gfx::PopClip() { rt->PopAxisAlignedClip(); }

ComPtr<ID2D1Bitmap> Gfx::BitmapFromPixels(int w, int h, const void* bgra) {
    ComPtr<ID2D1Bitmap> bmp;
    D2D1_BITMAP_PROPERTIES bp = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (rt) rt->CreateBitmap(D2D1::SizeU(w, h), bgra, w * 4, bp, &bmp);
    return bmp;
}

ComPtr<IWICBitmapSource> Gfx::LoadImageFile(const std::wstring& path, UINT maxW, UINT maxH) {
    ComPtr<IWICBitmapDecoder> dec;
    if (FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec)))
        return nullptr;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(dec->GetFrame(0, &frame))) return nullptr;
    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);
    if (!w || !h) return nullptr;
    ComPtr<IWICBitmapSource> src = frame;
    // scale so the image still covers maxW x maxH (no point keeping more pixels than the screen)
    double s = std::max((double)maxW / w, (double)maxH / h);
    if (s < 1.0) {
        ComPtr<IWICBitmapScaler> sc;
        if (SUCCEEDED(wic->CreateBitmapScaler(&sc)) &&
            SUCCEEDED(sc->Initialize(frame.Get(), std::max(1u, (UINT)(w * s + 0.5)), std::max(1u, (UINT)(h * s + 0.5)),
                                     WICBitmapInterpolationModeHighQualityCubic)))
            src = sc;
    }
    ComPtr<IWICFormatConverter> conv;
    if (FAILED(wic->CreateFormatConverter(&conv))) return nullptr;
    if (FAILED(conv->Initialize(src.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                WICBitmapPaletteTypeMedianCut)))
        return nullptr;
    ComPtr<IWICBitmap> mem;   // decode once, keep in memory
    if (FAILED(wic->CreateBitmapFromSource(conv.Get(), WICBitmapCacheOnLoad, &mem))) return nullptr;
    return mem;
}
