// RoomHome - meeting-room home screen. Shared includes and small helpers.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <d2d1effects.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <algorithm>

using Microsoft::WRL::ComPtr;

#define APP_NAME     L"RoomHome"
#define APP_VERSION  L"1.2.0"
#define APP_CLASS    L"RoomHome.MainWindow"

#if defined(_M_ARM64)
#define APP_ARCH L"ARM64"
#elif defined(_M_X64)
#define APP_ARCH L"x64"
#else
#define APP_ARCH L"x86"
#endif

// Private window messages.
#define WM_APP_NET        (WM_APP + 1)   // network snapshot updated
#define WM_APP_WIFIEVENT  (WM_APP + 2)   // lParam: WifiEvent* (receiver deletes)
#define WM_APP_AUDIO      (WM_APP + 3)   // volume changed outside the app
#define WM_APP_AUDIODEV   (WM_APP + 4)   // default playback device changed
#define WM_APP_LAUNCHED   (WM_APP + 5)   // lParam: LaunchMsg* (receiver deletes)
#define WM_APP_ICON       (WM_APP + 6)   // lParam: IconMsg* (receiver deletes)
#define WM_APP_QUIT       (WM_APP + 7)   // sent by --quit (installer)
#define WM_APP_EDITKEY    (WM_APP + 8)   // wParam: VK from a text field
#define WM_APP_PHOTO      (WM_APP + 9)   // lParam: PhotoMsg* (receiver deletes)

constexpr float kPi = 3.14159265358979f;

struct Box {
    float x = 0, y = 0, w = 0, h = 0;
    float r() const { return x + w; }
    float b() const { return y + h; }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    Box inset(float d) const { return { x + d, y + d, w - 2 * d, h - 2 * d }; }
    Box inset(float dx, float dy) const { return { x + dx, y + dy, w - 2 * dx, h - 2 * dy }; }
    Box scaled(float s) const { float nw = w * s, nh = h * s; return { cx() - nw / 2, cy() - nh / 2, nw, nh }; }
    Box moved(float dx, float dy) const { return { x + dx, y + dy, w, h }; }
    D2D1_RECT_F rect() const { return D2D1::RectF(x, y, x + w, y + h); }
};

inline Box Intersect(const Box& a, const Box& b) {
    float l = std::max(a.x, b.x), t = std::max(a.y, b.y);
    float r = std::min(a.r(), b.r()), btm = std::min(a.b(), b.b());
    if (r <= l || btm <= t) return { l, t, 0, 0 };
    return { l, t, r - l, btm - t };
}

inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float Saturate(float v) { return Clamp(v, 0.f, 1.f); }
inline float EaseOutCubic(float t) { t = Saturate(t); float u = 1 - t; return 1 - u * u * u; }
inline float EaseInOut(float t) { t = Saturate(t); return t * t * (3 - 2 * t); }
inline float EaseOutBack(float t) {
    t = Saturate(t);
    const float c1 = 1.70158f, c3 = c1 + 1;
    float u = t - 1;
    return 1 + c3 * u * u * u + c1 * u * u;
}

// ---- colours ----
inline D2D1_COLOR_F Rgb(uint32_t rgb, float a = 1.f) {
    return D2D1::ColorF(((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, a);
}
inline D2D1_COLOR_F WithA(D2D1_COLOR_F c, float a) { c.a = a; return c; }
inline D2D1_COLOR_F MulA(D2D1_COLOR_F c, float m) { c.a *= m; return c; }
inline D2D1_COLOR_F Mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float t) {
    return D2D1::ColorF(Lerp(a.r, b.r, t), Lerp(a.g, b.g, t), Lerp(a.b, b.b, t), Lerp(a.a, b.a, t));
}
inline D2D1_COLOR_F Lighten(D2D1_COLOR_F c, float t) {
    return D2D1::ColorF(Lerp(c.r, 1, t), Lerp(c.g, 1, t), Lerp(c.b, 1, t), c.a);
}
inline D2D1_COLOR_F Darken(D2D1_COLOR_F c, float t) {
    return D2D1::ColorF(c.r * (1 - t), c.g * (1 - t), c.b * (1 - t), c.a);
}
inline COLORREF ToColorRef(D2D1_COLOR_F c) {
    return RGB((int)(Saturate(c.r) * 255 + 0.5f), (int)(Saturate(c.g) * 255 + 0.5f), (int)(Saturate(c.b) * 255 + 0.5f));
}

// ---- widget ids ----
inline uint64_t HashBytes(const void* p, size_t n, uint64_t h = 1469598103934665603ULL) {
    auto b = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}
// splitmix64 finaliser: ids must differ in many bits, because animation keys are derived as
// id ^ small-constant (a weak mix once made two tiles share - and fight over - one key).
inline uint64_t Mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}
inline uint64_t Id(const char* s, uint64_t n = 0) {
    uint64_t h = Mix64(HashBytes(s, strlen(s)) ^ Mix64(n));
    return h ? h : 1;
}

// ---- strings / paths (util.cpp) ----
std::wstring Utf8ToW(const std::string& s);
std::string WToUtf8(const std::wstring& s);
std::wstring Format(const wchar_t* fmt, ...);
std::wstring ExePath();
std::wstring ExeDir();
std::wstring AppDataDir();          // %APPDATA%\RoomHome (created)
std::wstring ExpandEnv(const std::wstring& s);
bool FileExists(const std::wstring& p);
bool ReadFileBytes(const std::wstring& path, std::string& out);
bool WriteFileAtomic(const std::wstring& path, const std::string& data);
std::wstring Trim(const std::wstring& s);
std::wstring ToLower(std::wstring s);
std::wstring HResultText(HRESULT hr);
