// Immediate-mode UI core: hit testing, focus navigation, press / hold / drag tracking and animation
// values. Widgets register their boxes every frame; input is resolved against the previous frame.
#pragma once
#include "common.h"

struct UiItem {
    uint64_t id;
    Box box;
    int layer;
    bool focusable;
};

class UI {
public:
    // frame
    double now = 0;
    float dt = 0;
    int layer = 0;
    bool animating = false;

    // pointer (virtual coordinates)
    float px = -9999, py = -9999;
    bool pointerIn = false, pointerDown = false, touch = false, moved = false;
    float downX = 0, downY = 0;
    double downTime = 0;
    uint64_t hot = 0, active = 0;
    bool activeConsumed = false;
    uint64_t dragOwner = 0;
    float dragLastY = 0;
    float wheel = 0;

    // keyboard
    uint64_t focus = 0;
    bool keyboardMode = false;
    uint64_t keyActive = 0;
    double keyDownTime = 0;
    bool keyConsumed = false;

    void BeginFrame(double t, float delta);
    void EndFrame();
    void Add(uint64_t id, Box b, bool focusable = true);
    // previews draw real widgets without registering anything clickable
    bool suppress = false;
    // a second drawing of the same frame (theme reveal): animations read but do not advance
    bool freeze = false;
    // widgets drawn scaled by k around (cx, cy): hit boxes get the same scale
    void SetScale(float k, float cx, float cy) { xk_ = k; xcx_ = cx; xcy_ = cy; }
    void ClearScale() { xk_ = 1.f; }
    void PushClip(Box b) { clips_.push_back(clips_.empty() ? b : Intersect(clips_.back(), b)); }
    void PopClip() { if (!clips_.empty()) clips_.pop_back(); }

    bool Clicked(uint64_t id);
    bool RightClicked(uint64_t id);
    bool LongPressed(uint64_t id, float seconds = 0.6f);
    float HoldProgress(uint64_t id, float seconds);
    bool Released(uint64_t id) const { return released_ == id; }
    bool Hot(uint64_t id) const { return hot == id && !touch; }
    bool Pressed(uint64_t id) const { return (active == id && pointerDown && !activeConsumed) || keyActive == id; }
    bool Focused(uint64_t id) const { return keyboardMode && focus == id; }
    bool Highlighted(uint64_t id) const { return Hot(id) || Focused(id); }
    bool InteractiveLayer(int l) const { return l == prevTopLayer_; }
    void Scroll(uint64_t id, Box region, float* offset, float contentH);

    float Anim(uint64_t key, float target, float speed = 14.f);
    void AnimSet(uint64_t key, float v) { anim_[key] = v; }
    void KeepAnimating(const char* why = "keep") { animating = true; dbgWhy = why; }
    const char* dbgWhy = "";
    uint64_t dbgKey = 0;
    float dbgTarget = 0, dbgValue = 0;

    // input events
    void PointerMove(float x, float y, bool isTouch);
    void PointerDown(float x, float y, bool isTouch, bool right);
    void PointerUp(float x, float y, bool isTouch, bool right);
    void PointerLeave();
    void Wheel(float notches) { wheel += notches; }
    bool Key(int vk, bool down, bool repeat);     // returns true if consumed
    void ClearFocus() { focus = 0; }
    uint64_t HitTest(float x, float y) const;

private:
    std::vector<UiItem> items_, prev_;
    std::vector<Box> clips_;
    int topLayer_ = 0, prevTopLayer_ = 0;
    uint64_t clicked_ = 0, rightClicked_ = 0, released_ = 0, longFired_ = 0, holdFired_ = 0;
    std::unordered_map<uint64_t, float> anim_;
    float xk_ = 1.f, xcx_ = 0, xcy_ = 0;
    void MoveFocus(int dx, int dy);
    void TabFocus(bool back);
};

extern UI ui;

// ---- native text fields (EDIT controls) so IME, clipboard and the touch keyboard work ----
enum { EF_PASSWORD = 1, EF_NUMBER = 2, EF_AUTOFOCUS = 4, EF_MULTILINE = 8 };

class EditMgr {
public:
    HWND parent = nullptr;
    bool enabled = true;
    void Request(uint64_t id, Box virtualBox, std::wstring* value, const std::wstring& placeholder, int flags);
    void Sync(float scale, float offX, float offY, bool visible, COLORREF text, COLORREF bg, float fontPx);
    void Clear();
    void OnChange(HWND h);
    HBRUSH CtlColor(HDC dc, HWND h);
    bool Owns(HWND h) const;
    bool HasFocus(uint64_t id) const;
    bool AnyFocused() const;
    void SetText(uint64_t id, const std::wstring& t);
    void SetPasswordVisible(uint64_t id, bool show);
    void Focus(uint64_t id);
    void FocusNext(HWND from, bool back);
    uint64_t IdOf(HWND h) const;
    bool IsShown() const { return shown_; }

private:
    struct Field { HWND h; uint64_t id; std::wstring* value; int flags; bool used; bool autofocused; };
    struct Req { uint64_t id; Box box; std::wstring* value; std::wstring placeholder; int flags; };
    std::vector<Field> fields_;
    std::vector<Req> reqs_;
    HFONT font_ = nullptr;
    int fontPx_ = 0;
    HBRUSH bgBrush_ = nullptr;
    COLORREF bg_ = 0, text_ = 0;
    bool shown_ = false;
};

extern EditMgr edits;
