#pragma once
#include "common.h"

enum Face { FACE_DISPLAY, FACE_TEXT, FACE_ICON };
enum Align { AL_LEFT, AL_CENTER, AL_RIGHT };

// Weights
constexpr int W_LIGHT = 300, W_SEMILIGHT = 350, W_REGULAR = 400, W_SEMIBOLD = 600, W_BOLD = 700;

class Gfx {
public:
    ComPtr<ID2D1Factory1> factory;
    ComPtr<IDWriteFactory> dwrite;
    ComPtr<IWICImagingFactory> wic;
    ID2D1RenderTarget* rt = nullptr;          // current target (main or offscreen)
    ID2D1RenderTarget* mainRt = nullptr;
    ComPtr<ID2D1DeviceContext> dc;            // D2D 1.1 view of the main target (may be null)
    ComPtr<ID2D1SolidColorBrush> brush;
    ComPtr<ID2D1StrokeStyle> roundCap, dashed;
    uint32_t gen = 0;                         // bumps whenever device resources are recreated
    std::wstring famDisplay, famText, famIcon, famGdi;

    bool Init();
    void Bind(ID2D1RenderTarget* target);
    void Unbind();

    // text
    IDWriteTextLayout* Layout(const std::wstring& s, Face f, float size, int weight, Align al, float maxW, float maxH,
                              bool wrap = false, bool vcenter = true);
    float TextWidth(const std::wstring& s, Face f, float size, int weight);
    void Text(const std::wstring& s, Box b, Face f, float size, int weight, D2D1_COLOR_F c, Align al = AL_LEFT,
              bool vcenter = true, bool wrap = false);
    void Glyph(wchar_t ch, Box b, float size, D2D1_COLOR_F c);
    float Baseline(Face f, float size, int weight);   // distance from line top to baseline

    // shapes
    ID2D1SolidColorBrush* Solid(D2D1_COLOR_F c) { brush->SetColor(c); return brush.Get(); }
    void FillRect(Box b, D2D1_COLOR_F c, float radius = 0);
    void FillRectBrush(Box b, ID2D1Brush* br, float radius = 0);
    void StrokeRect(Box b, D2D1_COLOR_F c, float radius, float width, bool dash = false);
    void FillCircle(float cx, float cy, float r, D2D1_COLOR_F c);
    void StrokeCircle(float cx, float cy, float r, D2D1_COLOR_F c, float width);
    void Line(float x1, float y1, float x2, float y2, D2D1_COLOR_F c, float width);
    void Arc(float cx, float cy, float r, float a0Deg, float a1Deg, D2D1_COLOR_F c, float width);
    void Shadow(Box b, float radius, float blur, float alpha, float dy, D2D1_COLOR_F col);
    ID2D1LinearGradientBrush* LinearV(Box b, D2D1_COLOR_F top, D2D1_COLOR_F bottom);
    ID2D1RadialGradientBrush* Radial(float cx, float cy, float rx, float ry, D2D1_COLOR_F c);   // soft falloff to clear
    ID2D1BitmapBrush* Noise();

    // state
    void PushOpacity(float o);
    void PopOpacity();
    void PushClip(Box b);
    void PopClip();
    void PushMask(ID2D1Geometry* geo);   // geometry in the coordinate space active when pushed
    void PopMask() { PopOpacity(); }

    // images
    ComPtr<ID2D1Bitmap> BitmapFromPixels(int w, int h, const void* bgraPremul);
    ComPtr<ID2D1Bitmap> BitmapFromBgrx(int w, int h, const void* bgrx);   // alpha bytes ignored (camera pictures)
    ComPtr<IWICBitmapSource> LoadImageFile(const std::wstring& path, UINT maxW, UINT maxH);

    void ClearTextCaches() { fmts_.clear(); layouts_.clear(); }
    // While set, text is drawn exactly as given (user content: field values, the room name);
    // otherwise a string that is a known English label is shown in the current language.
    bool literal = false;

private:
    struct FmtKey { int face, size10, weight, align, wrap, vc, lang; bool operator==(const FmtKey& o) const { return memcmp(this, &o, sizeof(*this)) == 0; } };
    struct FmtHash { size_t operator()(const FmtKey& k) const { return (size_t)HashBytes(&k, sizeof(k)); } };
    std::unordered_map<FmtKey, ComPtr<IDWriteTextFormat>, FmtHash> fmts_;
    std::unordered_map<uint64_t, ComPtr<IDWriteTextLayout>> layouts_;
    std::unordered_map<uint64_t, ComPtr<ID2D1LinearGradientBrush>> linear_;
    std::unordered_map<uint64_t, ComPtr<ID2D1RadialGradientBrush>> radial_;
    ComPtr<ID2D1BitmapBrush> noise_;
    ComPtr<IDWriteTypography> tnum_;
    std::vector<ComPtr<ID2D1Layer>> layers_;
    int layerDepth_ = 0;
    IDWriteTextFormat* Format(Face f, float size, int weight, Align al, bool wrap, bool vcenter);
};

extern Gfx gfx;
