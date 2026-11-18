#pragma once
#include "common.h"

// One app button in the dock.
//   kind: zoom | teams | webex | web | edge | chrome | explorer | cast | project | program
//   target: URL (web) or path / shell:AppsFolder\AUMID / App Paths name (program)
//   glyph: key from kGlyphs, or "appicon" to show the program's own icon
struct DockItem {
    uint32_t uid = 0;          // runtime only
    std::wstring name, kind, target, args, glyph;
    uint32_t color = 0x3D7BFF;
};

// One widget in the centre row.
//   type: analog | digital | calendar | worldclock | note
struct WidgetCfg {
    uint32_t uid = 0;          // runtime only
    std::wstring type;
    bool seconds = true;       // analog second hand / digital + world clock seconds
    bool numbers = true;       // analog: 1-12 numerals
    bool date = true;          // analog date tag / digital date line
    bool greeting = true;      // digital: "Good morning"
    bool weekNumbers = false;  // calendar
    bool large = false;        // digital size, note text size
    bool analog = false;       // world clock style
    std::wstring city, zone;   // world clock (zone = Windows time zone key, e.g. "Tokyo Standard Time")
    std::wstring title, text;  // note
    uint32_t color = 0;        // note tint, 0 = plain glass
    // photo
    std::wstring path;         // a picture, or a folder for a slideshow
    bool folder = false;
    int interval = 30;         // slideshow seconds
    bool shuffle = false;
    int shape = 0;             // 0 landscape, 1 square, 2 portrait
    bool fit = false;          // false = fill (crop), true = whole picture
    std::wstring caption;
};
constexpr int kMaxWidgets = 4;

struct WidgetType { const wchar_t* type; const wchar_t* name; const wchar_t* desc; };
extern const WidgetType kWidgetTypes[];
extern const int kWidgetTypeCount;
extern const uint32_t kNoteColors[];
extern const int kNoteColorCount;
const WidgetType* WidgetTypeOf(const std::wstring& type);
WidgetCfg NewWidget(const std::wstring& type);

struct Config {
    std::wstring roomName;               // empty -> "Meeting Room" in the UI language (RoomTitle())
    std::wstring subtitle;               // empty -> show the PC's IP address
    int clockMode = 0;                   // 0 auto (Windows), 1 24-hour, 2 12-hour
    int weekStart = 0;                   // 0 auto (Windows), 1 Sunday, 2 Monday
    int theme = 0;                       // 0 auto (Windows), 1 dark, 2 light
    int language = 0;                    // 0 auto (installer choice / Windows), 1 English, 2 Thai
    uint32_t accent = 0x0A84FF;
    std::wstring wallpaperDark, wallpaperLight;   // empty -> built-in aurora
    bool keepAwake = false;
    int idleMinutes = 15;                // ambient clock after N idle minutes, 0 = off
    int dockCapacity = 6;
    int volumeStep = 5;                  // percent per +/- press
    std::wstring adminPin;               // digits; empty = no lock
    int monitor = 0;                     // 0 = primary, N = Nth display (1-based)
    std::wstring camera, cameraId;       // the room camera (name, "USB\VID_xxxx&PID_xxxx"); empty = not chosen yet
    bool cameraMirror = true;            // show the test picture like a mirror, as meeting apps do
    std::vector<DockItem> dock;
    std::vector<WidgetCfg> widgets;
};

struct Preset {
    const wchar_t* name;
    const wchar_t* kind;
    const wchar_t* target;
    const wchar_t* glyph;
    uint32_t color;
    const wchar_t* note;     // shown in the editor
};
extern const Preset kPresets[];
extern const int kPresetCount;

struct GlyphDef { const wchar_t* key; wchar_t ch; };
extern const GlyphDef kGlyphs[];
extern const int kGlyphCount;
wchar_t GlyphChar(const std::wstring& key);

extern const uint32_t kSwatches[];
extern const int kSwatchCount;
extern const uint32_t kAccents[];
extern const int kAccentCount;

uint32_t NewUid();
DockItem ItemFromPreset(const Preset& p);
const Preset* PresetForKind(const std::wstring& kind);
void ConfigDefaults(Config& c);
std::wstring ConfigPath();
bool ConfigLoad(Config& c, const std::wstring& pathOverride = L"");
bool ConfigSave(const Config& c);
bool IsCustomKind(const std::wstring& kind);   // web/program: user-provided target
