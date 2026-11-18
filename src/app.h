#pragma once
#include "common.h"
#include "config.h"
#include "system.h"
#include "camera.h"
#include "gfx.h"
#include "ui.h"

enum class Sheet { None, Wifi, Network, Settings, Power, AddApp, EditApp, Rename, Pin, AddWidget, EditWidget, Camera };

struct Theme {
    D2D1_COLOR_F text, text2, text3, glass, glassHi, glassStroke, sheet, field, shadow, dim, accent, onAccent,
                 danger, success, warn, track, tileGlass;
    float light = 0;
};

struct Toast {
    uint64_t id = 0;
    std::wstring text;
    int kind = 0;           // 0 info, 1 success, 2 error
    double born = 0, life = 3.5;
    std::wstring actionLabel;
    std::function<void()> action;
};

struct IconEntry {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    ComPtr<ID2D1Bitmap> bmp;
    uint32_t gen = 0;
    bool loading = false, failed = false;
};

struct PhotoEntry {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    ComPtr<ID2D1Bitmap> bmp;
    uint32_t gen = 0;
    bool loading = false, failed = false;
    double lastUsed = 0;
};

struct WallEntry {
    std::wstring path;
    ComPtr<IWICBitmapSource> src;
    ComPtr<ID2D1Bitmap> bmp, blur;
    uint32_t gen = 0;
    int blurW = 0, blurH = 0;
    bool failed = false;
};

struct WifiUi {
    std::string selected;       // raw SSID of the expanded row
    std::wstring password;
    bool showPassword = false;
    std::wstring status;
    int statusKind = 0;
    float scroll = 0;
};

struct EditUi {
    DockItem item;
    int index = -1;
    bool isNew = false;
};

struct WidgetEditUi {
    WidgetCfg w;
    int index = -1;
    std::wstring tzFilter;
    std::wstring lastAutoCity;
    float tzScroll = 0;
};

struct PinUi {
    std::wstring entered;
    std::function<void()> then;
    double shakeAt = -10;
    bool wrong = false;
};

// the camera test sheet
struct CamUi {
    std::wstring name, link;      // the camera being shown
    CamPreviewState st;
    std::vector<uint8_t> px;      // newest picture, BGRX top-down
    int pw = 0, ph = 0;
    uint32_t seq = 0, bmpSeq = 0, bmpGen = 0;
    ComPtr<ID2D1Bitmap> bmp;
    double startedAt = 0;
    bool paused = false;          // stopped while another app is in front
    bool demo = false;            // screenshots: a test pattern instead of a camera
};

// the last live test, which the camera light on the home screen also shows
struct CamTest {
    std::wstring name;
    int result = 0;               // 0 not tested, 1 worked, 2 failed
    int fail = 0;                 // CAMFAIL_*
    HRESULT hr = S_OK;
};

struct App {
    HWND hwnd = nullptr;
    Config cfg;
    bool screenshot = false, dryRun = false, windowed = false, allowClose = false;

    // geometry: virtual units are 1080 tall; s = device px per unit
    float W = 1920, H = 1080, s = 1, vw = 1920, vh = 1080;
    float shiftX = 0, shiftY = 0;
    int shiftPhase = 0;

    double now = 0;
    SYSTEMTIME st{};
    int shownSec = -1, prevSec = -1;
    double secChangedAt = -10;

    bool systemLight = false;
    float themeT = 0;            // 0 dark, 1 light: the theme of the pass being drawn
    Theme th;
    // light/dark switch: the new theme grows as a circle from the sun/moon button
    bool themeInited = false, lightShown = false, lightTo = false, themeAnimating = false;
    double themeStart = 0;
    float themeProg = 0;
    float themeOriginX = 0, themeOriginY = 0;   // virtual coordinates

    NetInfo net;
    CamStatus cams;
    CamUi cam;
    CamTest camTest;
    bool audioOk = false, muted = false;
    float volume = 0;
    std::wstring audioDevice;

    Sheet sheet = Sheet::None;
    bool sheetOpen = false;
    float sheetT = 0;

    bool editMode = false;
    uint32_t dragUid = 0;
    float dragGrabX = 0, dragGrabY = 0;
    int dragInsert = -1;

    uint32_t wdragUid = 0;
    float wdragGrabX = 0;
    int wdragInsert = 0;

    WifiUi wifi;
    EditUi edit;
    WidgetEditUi wedit;
    PinUi pin;
    std::wstring renameTitle, renameSubtitle;
    std::wstring pinDraft;

    std::vector<Toast> toasts;
    std::vector<std::function<void()>> deferred;
    std::unordered_map<std::wstring, IconEntry> icons;
    std::unordered_map<uint32_t, std::wstring> iconKeys;   // uid -> resolved icon source
    std::unordered_map<std::wstring, PhotoEntry> photos;   // "path|WxH" -> decoded picture
    WallEntry wallDark, wallLight;

    float ambient = 0;
    bool ambientOn = false;
    bool shuttingDown = false;
    std::wstring shutdownText;
    double lastLaunch = -10;
    uint32_t lastLaunchUid = 0;
    int calOffset = 0;
    double calTouched = 0;
    bool autostartUser = false, autostartMachine = false;
    double unlockedUntil = -1;
    double lastTimer = 0;
};

extern App g;

// main.cpp
void RequestFrame();
void Defer(std::function<void()> fn);
void SaveConfig();
void ShowToast(const std::wstring& text, int kind = 0, const std::wstring& actionLabel = L"",
               std::function<void()> action = nullptr);
void ApplyKeepAwake();
void ApplyLanguage();
std::wstring RoomTitle();   // the room name, or "Meeting Room" in the UI language when none is set
void RefreshAutostart();
bool IsLight();
void LaunchItem(const DockItem& it);
ID2D1Bitmap* IconFor(const DockItem& it);
void InvalidateIcon(uint32_t uid);
void SetSheetEditsVisible(bool v);

// widgets.cpp
Theme MakeTheme(float t, uint32_t accent);
void Glass(Box b, float radius, float hover = 0, bool shadow = true, float alpha = 1);
ID2D1Brush* GlassBlurBrush();
bool Button(uint64_t id, Box b, const std::wstring& label, int style = 0, wchar_t glyph = 0, bool enabled = true);
bool IconButton(uint64_t id, Box b, wchar_t glyph, int style = 0, float glyphSize = 0, bool enabled = true);
bool Toggle(uint64_t id, Box b, bool on);
int Segmented(uint64_t id, Box b, const std::vector<std::wstring>& opts, int sel);
bool Swatch(uint64_t id, Box b, D2D1_COLOR_F c, bool selected);
void Field(uint64_t id, Box b, std::wstring* value, const std::wstring& placeholder, int flags = 0);
void FocusRing(uint64_t id, Box b, float radius);
void DrawWifiIcon(float cx, float cy, float size, int level, D2D1_COLOR_F on, D2D1_COLOR_F off);
void DrawTileFace(const DockItem& it, Box b, float hl, float hx, float hy);
enum { BTN_GLASS = 0, BTN_ACCENT = 1, BTN_DANGER = 2, BTN_PLAIN = 3 };

// home.cpp
void DrawBackground();
void DrawHome(float alpha);
void DrawAmbient();
void DrawToasts();
void DrawShutdown();
float DockTop();
std::wstring SpeedText(uint64_t bps);
ID2D1Bitmap* WallpaperBitmap(bool light);
struct WSize { float w, h; };
WSize WidgetSize(const WidgetCfg& w);
void DrawWidget(const WidgetCfg& w, Box b);                  // b = natural-size box
void DrawWidgetPreview(const WidgetCfg& w, Box area);        // scaled to fit, not clickable
void EnterEditMode();

// sheets.cpp
void OpenSheet(Sheet s);
void CloseSheet();
void DrawSheets();
void RequireUnlock(std::function<void()> then);
void OnWifiEvent(const WifiEvent& e);
void OnEditKey(int vk, HWND from);
void OnPinChar(wchar_t c);
void OpenEditor(int index, const DockItem& item, bool isNew);
void RemoveDockItem(int index);
void SetTheme(int theme);
void OpenWidgetEditor(int index);
void RemoveWidget(int index);
// camera (sheets.cpp)
const CamDevice* RoomCamera(const CamStatus& s, std::wstring* missingName = nullptr);
int CameraLight(std::wstring* title = nullptr, std::wstring* detail = nullptr);   // 0 checking, 1 works, 2 not working
void OnCamerasChanged();      // WM_APP_CAMERA
void OnCameraFrame();         // WM_APP_CAMFRAME
void CameraAppActive(bool active);
void CameraSheetGone();       // the sheet has finished closing: drop the last picture
void DemoCamera();            // screenshots: a test pattern for the preview
