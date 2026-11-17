#include "ui.h"
#include "gfx.h"
#include "lang.h"
#include <commctrl.h>

UI ui;
EditMgr edits;

constexpr float kMoveSlop = 14.f;

void UI::BeginFrame(double t, float delta) {
    now = t;
    dt = delta;
    animating = false;
    dbgWhy = "";
    layer = 0;
    topLayer_ = 0;
    items_.clear();
    clips_.clear();
    // long press / hold need frames while a pointer is down
    if (pointerDown || keyActive) { animating = true; dbgWhy = pointerDown ? "pointerDown" : "keyActive"; }
}

void UI::EndFrame() {
    prev_.swap(items_);
    prevTopLayer_ = topLayer_;
    clicked_ = rightClicked_ = released_ = 0;
    wheel = 0;
    if (!touch && pointerIn && !pointerDown) hot = HitTest(px, py);
    // keep keyboard focus on something that still exists
    if (focus) {
        bool found = false;
        for (auto& it : prev_)
            if (it.id == focus && it.layer == prevTopLayer_) { found = true; break; }
        if (!found) focus = 0;
    }
}

void UI::Add(uint64_t id, Box b, bool focusable) {
    if (suppress) return;
    if (xk_ != 1.f) b = { (b.x - xcx_) * xk_ + xcx_, (b.y - xcy_) * xk_ + xcy_, b.w * xk_, b.h * xk_ };
    if (!clips_.empty()) {
        Box c = clips_.back();
        if (b.y < c.y - 0.5f || b.b() > c.b() + 0.5f || b.x < c.x - 0.5f || b.r() > c.r() + 0.5f) {
            Box i = Intersect(b, c);
            if (i.w <= 0 || i.h <= 0) return;
            b = i;
            focusable = false;   // partly scrolled out: clickable, but keyboard focus skips it
        }
    }
    items_.push_back({ id, b, layer, focusable });
    topLayer_ = std::max(topLayer_, layer);
}

uint64_t UI::HitTest(float x, float y) const {
    for (auto it = prev_.rbegin(); it != prev_.rend(); ++it)
        if (it->layer == prevTopLayer_ && it->box.contains(x, y)) return it->id;
    return 0;
}

bool UI::Clicked(uint64_t id) {
    if (clicked_ == id && id) { clicked_ = 0; return true; }
    return false;
}

bool UI::RightClicked(uint64_t id) {
    if (rightClicked_ == id && id) { rightClicked_ = 0; return true; }
    return false;
}

bool UI::LongPressed(uint64_t id, float seconds) {
    if (active == id && pointerDown && !moved && !activeConsumed && now - downTime >= seconds) {
        activeConsumed = true;
        longFired_ = id;
        return true;
    }
    return false;
}

float UI::HoldProgress(uint64_t id, float seconds) {
    float p = 0;
    if (active == id && pointerDown && !activeConsumed && holdFired_ != id) p = (float)((now - downTime) / seconds);
    else if (keyActive == id && !keyConsumed && holdFired_ != id) p = (float)((now - keyDownTime) / seconds);
    if (p >= 1.f) {
        holdFired_ = id;
        if (active == id) activeConsumed = true;
        if (keyActive == id) keyConsumed = true;
        return 1.f;
    }
    if (p > 0) { animating = true; dbgWhy = "hold"; }
    return Saturate(p);
}

void UI::Scroll(uint64_t id, Box region, float* offset, float contentH) {
    float maxOff = std::max(0.f, contentH - region.h);
    if (wheel != 0 && region.contains(px, py)) {
        *offset -= wheel * 90.f;
        wheel = 0;
    }
    if (pointerDown && moved && region.contains(downX, downY) && (dragOwner == 0 || dragOwner == id) && maxOff > 0) {
        if (dragOwner == 0) {
            dragOwner = id;
            activeConsumed = true;
            dragLastY = downY;
        }
        *offset -= (py - dragLastY);
        dragLastY = py;
    }
    *offset = Clamp(*offset, 0, maxOff);
}

float UI::Anim(uint64_t key, float target, float speed) {
    auto it = anim_.find(key);
    if (freeze) return it == anim_.end() ? target : it->second;
    if (it == anim_.end()) { anim_[key] = target; return target; }
    float& v = it->second;
    float k = 1.f - std::exp(-dt * speed);
    v += (target - v) * k;
    if (std::fabs(target - v) < 0.0015f) v = target;
    else { animating = true; dbgWhy = "anim"; dbgKey = key; dbgTarget = target; dbgValue = v; }
    return v;
}

void UI::PointerMove(float x, float y, bool isTouch) {
    px = x; py = y;
    pointerIn = true;
    touch = isTouch;
    if (pointerDown && !moved && (std::fabs(x - downX) > kMoveSlop || std::fabs(y - downY) > kMoveSlop)) moved = true;
    if (!isTouch) {
        uint64_t h = pointerDown ? hot : HitTest(x, y);
        if (h != hot) { hot = h; }
        keyboardMode = false;
    }
}

void UI::PointerDown(float x, float y, bool isTouch, bool right) {
    px = x; py = y;
    pointerIn = true;
    touch = isTouch;
    keyboardMode = false;
    uint64_t h = HitTest(x, y);
    if (right) { active = 0; rightClicked_ = h; return; }
    pointerDown = true;
    moved = false;
    downX = x; downY = y;
    downTime = now;
    active = h;
    activeConsumed = false;
    dragOwner = 0;
    holdFired_ = 0;
    longFired_ = 0;
    if (!isTouch) hot = h;
    if (h) focus = h;
}

void UI::PointerUp(float x, float y, bool isTouch, bool right) {
    if (right) return;
    px = x; py = y;
    if (!pointerDown) return;
    pointerDown = false;
    uint64_t h = HitTest(x, y);
    if (active && h == active && !activeConsumed && !moved) clicked_ = active;
    released_ = active;
    active = 0;
    dragOwner = 0;
    if (isTouch) { pointerIn = false; hot = 0; px = py = -9999; }
    else hot = h;
}

void UI::PointerLeave() {
    pointerIn = false;
    if (!pointerDown) { hot = 0; px = py = -9999; }
}

void UI::MoveFocus(int dx, int dy) {
    const UiItem* cur = nullptr;
    for (auto& it : prev_)
        if (it.id == focus && it.layer == prevTopLayer_) cur = &it;
    if (!cur) {
        for (auto& it : prev_)
            if (it.focusable && it.layer == prevTopLayer_) { focus = it.id; return; }
        return;
    }
    float cx = cur->box.cx(), cy = cur->box.cy();
    const UiItem* best = nullptr;
    float bestScore = 1e30f;
    for (auto& it : prev_) {
        if (!it.focusable || it.layer != prevTopLayer_ || it.id == cur->id) continue;
        float ox = it.box.cx() - cx, oy = it.box.cy() - cy;
        float along = dx ? ox * dx : oy * dy;
        if (along <= 4.f) continue;
        // overlap on the cross axis is preferred
        float cross;
        if (dx) cross = std::max(0.f, std::max(it.box.y - cur->box.b(), cur->box.y - it.box.b()));
        else cross = std::max(0.f, std::max(it.box.x - cur->box.r(), cur->box.x - it.box.r()));
        float off = dx ? std::fabs(oy) : std::fabs(ox);
        float score = along + cross * 4.f + off * 0.35f;
        if (score < bestScore) { bestScore = score; best = &it; }
    }
    if (best) focus = best->id;
}

void UI::TabFocus(bool back) {
    std::vector<const UiItem*> f;
    for (auto& it : prev_)
        if (it.focusable && it.layer == prevTopLayer_) f.push_back(&it);
    if (f.empty()) return;
    int idx = -1;
    for (int i = 0; i < (int)f.size(); i++)
        if (f[i]->id == focus) idx = i;
    int n = (int)f.size();
    idx = idx < 0 ? (back ? n - 1 : 0) : (idx + (back ? n - 1 : 1)) % n;
    focus = f[idx]->id;
}

bool UI::Key(int vk, bool down, bool repeat) {
    if (down) {
        switch (vk) {
        case VK_LEFT: keyboardMode = true; MoveFocus(-1, 0); return true;
        case VK_RIGHT: keyboardMode = true; MoveFocus(1, 0); return true;
        case VK_UP: keyboardMode = true; MoveFocus(0, -1); return true;
        case VK_DOWN: keyboardMode = true; MoveFocus(0, 1); return true;
        case VK_TAB: keyboardMode = true; TabFocus(GetKeyState(VK_SHIFT) < 0); return true;
        case VK_RETURN:
        case VK_SPACE:
            if (!keyboardMode || !focus) { keyboardMode = true; if (!focus) MoveFocus(1, 0); return true; }
            if (!repeat) { keyActive = focus; keyDownTime = now; keyConsumed = false; holdFired_ = 0; }
            return true;
        }
        return false;
    }
    if (vk == VK_RETURN || vk == VK_SPACE) {
        if (keyActive && keyActive == focus && !keyConsumed) clicked_ = keyActive;
        keyActive = 0;
        return true;
    }
    return false;
}

// ======================================================================
// EditMgr
// ======================================================================
static LRESULT CALLBACK EditSubclass(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_KEYDOWN: {
        bool multi = (GetWindowLongW(h, GWL_STYLE) & ES_MULTILINE) != 0;
        if (wp == VK_RETURN && multi && GetKeyState(VK_CONTROL) >= 0) break;   // new line; Ctrl+Enter saves
        if (wp == VK_RETURN || wp == VK_ESCAPE || wp == VK_TAB) {
            PostMessageW(GetParent(h), WM_APP_EDITKEY, wp, (LPARAM)h);
            return 0;
        }
        if (wp == 'A' && GetKeyState(VK_CONTROL) < 0) { SendMessageW(h, EM_SETSEL, 0, -1); return 0; }
        break;
    }
    case WM_CHAR: {
        bool multi = (GetWindowLongW(h, GWL_STYLE) & ES_MULTILINE) != 0;
        if (wp == L'\r' && multi && GetKeyState(VK_CONTROL) >= 0) break;
        if (wp == L'\r' || wp == 27 || wp == L'\t' || wp == 1 /* Ctrl+A */ || wp == 10 /* Ctrl+Enter */) return 0;   // no beep
        break;
    }
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | DefSubclassProc(h, msg, wp, lp);
    }
    return DefSubclassProc(h, msg, wp, lp);
}

void EditMgr::Request(uint64_t id, Box b, std::wstring* value, const std::wstring& placeholder, int flags) {
    if (ui.suppress) return;
    reqs_.push_back({ id, b, value, placeholder, flags });
}

void EditMgr::Sync(float scale, float offX, float offY, bool visible, COLORREF text, COLORREF bg, float fontPx) {
    if (!enabled || !parent) { reqs_.clear(); return; }
    int fpx = std::max(10, (int)std::lround(fontPx));
    if (!font_ || fpx != fontPx_) {
        HFONT nf = CreateFontW(-fpx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, gfx.famGdi.c_str());
        for (auto& f : fields_) SendMessageW(f.h, WM_SETFONT, (WPARAM)nf, TRUE);
        if (font_) DeleteObject(font_);
        font_ = nf;
        fontPx_ = fpx;
    }
    if (!bgBrush_ || bg != bg_ || text != text_) {
        if (bgBrush_) DeleteObject(bgBrush_);
        bgBrush_ = CreateSolidBrush(bg);
        bg_ = bg;
        text_ = text;
        for (auto& f : fields_) InvalidateRect(f.h, nullptr, TRUE);
    }
    for (auto& f : fields_) f.used = false;
    HDWP dwp = BeginDeferWindowPos((int)reqs_.size() + 1);
    for (auto& r : reqs_) {
        Field* f = nullptr;
        for (auto& x : fields_) if (x.id == r.id) { f = &x; break; }
        if (!f) {
            DWORD style = WS_CHILD | ES_LEFT;
            if (r.flags & EF_MULTILINE) style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN;
            else style |= ES_AUTOHSCROLL;
            if (r.flags & EF_PASSWORD) style |= ES_PASSWORD;
            if (r.flags & EF_NUMBER) style |= ES_NUMBER;
            std::wstring init = r.value ? *r.value : L"";
            if (r.flags & EF_MULTILINE) {   // the EDIT control needs CR LF line breaks
                std::wstring t;
                for (size_t i = 0; i < init.size(); i++) {
                    if (init[i] == L'\n' && (i == 0 || init[i - 1] != L'\r')) t += L'\r';
                    t += init[i];
                }
                init = t;
            }
            HWND h = CreateWindowExW(0, L"EDIT", init.c_str(), style, 0, 0, 10, 10, parent, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
            if (!h) continue;
            SendMessageW(h, WM_SETFONT, (WPARAM)font_, FALSE);
            SendMessageW(h, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
            SendMessageW(h, EM_LIMITTEXT, 512, 0);
            if (!r.placeholder.empty()) { std::wstring cue = Tr(r.placeholder); SendMessageW(h, EM_SETCUEBANNER, TRUE, (LPARAM)cue.c_str()); }   // TRUE: also while focused (an empty room name shows its default)
            SetWindowSubclass(h, EditSubclass, 1, 0);
            fields_.push_back({ h, r.id, r.value, r.flags, false, false });
            f = &fields_.back();
        }
        f->value = r.value;
        f->used = true;
        int x = (int)std::lround(r.box.x * scale + offX);
        int w = (int)std::lround(r.box.w * scale);
        int lineH = (int)std::lround(fpx * 1.36f);
        int y = (int)std::lround(r.box.cy() * scale + offY - lineH / 2.f);
        if (r.flags & EF_MULTILINE) {
            y = (int)std::lround(r.box.y * scale + offY);
            lineH = std::max(lineH, (int)std::lround(r.box.h * scale));
        }
        UINT fl = SWP_NOZORDER | SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
        if (dwp) dwp = DeferWindowPos(dwp, f->h, nullptr, x, y, std::max(10, w), lineH, fl);
        if (visible && (r.flags & EF_AUTOFOCUS) && !f->autofocused) {
            f->autofocused = true;
            SetFocus(f->h);
            SendMessageW(f->h, EM_SETSEL, 0, -1);
        }
    }
    for (auto& f : fields_)
        if (!f.used && IsWindowVisible(f.h) && dwp) dwp = DeferWindowPos(dwp, f.h, nullptr, 0, 0, 0, 0,
                                                                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
    if (dwp) EndDeferWindowPos(dwp);
    HWND focused = GetFocus();
    if (focused && Owns(focused) && (!visible || !IsWindowVisible(focused))) SetFocus(parent);
    shown_ = visible && !reqs_.empty();
    reqs_.clear();
}

void EditMgr::Clear() {
    HWND focused = GetFocus();
    for (auto& f : fields_) {
        if (focused == f.h) SetFocus(parent);
        RemoveWindowSubclass(f.h, EditSubclass, 1);
        DestroyWindow(f.h);
    }
    fields_.clear();
    reqs_.clear();
    shown_ = false;
}

void EditMgr::OnChange(HWND h) {
    for (auto& f : fields_) {
        if (f.h != h || !f.value) continue;
        int n = GetWindowTextLengthW(h);
        std::wstring s(n + 1, L'\0');
        GetWindowTextW(h, s.data(), n + 1);
        s.resize(n);
        *f.value = s;
    }
}

HBRUSH EditMgr::CtlColor(HDC dc, HWND h) {
    if (!Owns(h)) return nullptr;
    SetTextColor(dc, text_);
    SetBkColor(dc, bg_);
    return bgBrush_;
}

bool EditMgr::Owns(HWND h) const {
    for (auto& f : fields_) if (f.h == h) return true;
    return false;
}

uint64_t EditMgr::IdOf(HWND h) const {
    for (auto& f : fields_) if (f.h == h) return f.id;
    return 0;
}

bool EditMgr::HasFocus(uint64_t id) const {
    HWND fh = GetFocus();
    for (auto& f : fields_) if (f.id == id) return f.h == fh;
    return false;
}

bool EditMgr::AnyFocused() const { return Owns(GetFocus()); }

void EditMgr::SetText(uint64_t id, const std::wstring& t) {
    for (auto& f : fields_)
        if (f.id == id) SetWindowTextW(f.h, t.c_str());
}

void EditMgr::SetPasswordVisible(uint64_t id, bool show) {
    for (auto& f : fields_)
        if (f.id == id) {
            SendMessageW(f.h, EM_SETPASSWORDCHAR, show ? 0 : (WPARAM)0x25CF, 0);
            InvalidateRect(f.h, nullptr, TRUE);
        }
}

void EditMgr::Focus(uint64_t id) {
    for (auto& f : fields_)
        if (f.id == id && IsWindowVisible(f.h)) {
            SetFocus(f.h);
            SendMessageW(f.h, EM_SETSEL, 0, -1);
        }
}

void EditMgr::FocusNext(HWND from, bool back) {
    std::vector<HWND> vis;
    for (auto& f : fields_) if (IsWindowVisible(f.h)) vis.push_back(f.h);
    if (vis.empty()) return;
    int i = 0;
    for (int k = 0; k < (int)vis.size(); k++) if (vis[k] == from) i = k;
    int n = (int)vis.size();
    HWND next = vis[(i + (back ? n - 1 : 1)) % n];
    SetFocus(next);
    SendMessageW(next, EM_SETSEL, 0, -1);
}
