#include "config.h"
#include "json.h"
#include "lang.h"

const Preset kPresets[] = {
    { L"Zoom",            L"zoom",     L"",                        L"video",     0x2D8CFF, L"Opens the Zoom app. If Zoom is not installed, Zoom opens on the web in Microsoft Edge." },
    { L"Microsoft Teams", L"teams",    L"",                        L"people",    0x5B5FC7, L"Opens the Teams app (new or classic). Without it, Teams opens on the web in Microsoft Edge." },
    { L"Google Meet",     L"web",      L"https://meet.google.com", L"videochat", 0x00A862, L"Opens meet.google.com in Microsoft Edge." },
    { L"Webex",           L"webex",    L"",                        L"headset",   0x1D9BC9, L"Opens the Webex app. If it is not installed, Webex opens on the web in Microsoft Edge." },
    { L"Microsoft Edge",  L"edge",     L"",                        L"globe",     0x0F78D4, L"Opens a new Microsoft Edge window." },
    { L"Google Chrome",   L"chrome",   L"",                        L"world",     0xDB4437, L"Opens a new Google Chrome window." },
    { L"File Explorer",   L"explorer", L"",                        L"folder",    0xE8A317, L"Opens This PC, so USB drives are one tap away." },
    { L"Cast screen",     L"cast",     L"",                        L"cast",      0x8E5CF7, L"Opens the Windows Cast panel (Win+K) to send this screen to a wireless display." },
    { L"Display mode",    L"project",  L"",                        L"tv",        0x5E6AD2, L"Opens the Windows Project panel (Win+P): duplicate, extend or second screen only." },
    { L"PowerPoint",      L"program",  L"powerpnt.exe",            L"slides",    0xD35230, L"Starts PowerPoint (found through Windows App Paths)." },
    { L"Website",         L"web",      L"",                        L"link",      0x30B35A, L"Any web address, opened in Microsoft Edge." },
    { L"Program",         L"program",  L"",                        L"appicon",   0x7D7D86, L"Any program, shortcut or file. Arguments are optional." },
};
const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

// Segoe Fluent Icons code points (verified against the font).
const GlyphDef kGlyphs[] = {
    { L"video", 0xE714 }, { L"videochat", 0xE8AA }, { L"people", 0xE716 }, { L"group", 0xE902 },
    { L"headset", 0xE95B }, { L"phone", 0xE717 }, { L"chat", 0xE8BD }, { L"mail", 0xE715 },
    { L"globe", 0xE774 }, { L"world", 0xE909 }, { L"link", 0xE71B }, { L"webapp", 0xEC27 },
    { L"folder", 0xE8B7 }, { L"document", 0xE8A5 }, { L"slides", 0xE786 }, { L"calendar", 0xE787 },
    { L"cast", 0xEC15 }, { L"tv", 0xE7F4 }, { L"laptop", 0xE7F8 }, { L"pen", 0xE70F },
    { L"camera", 0xE722 }, { L"mic", 0xE720 }, { L"music", 0xE8D6 }, { L"film", 0xE8B2 },
    { L"photo", 0xE91B }, { L"apps", 0xECA5 }, { L"home", 0xE80F }, { L"idea", 0xEA80 },
    { L"code", 0xE943 }, { L"print", 0xE749 }, { L"settings", 0xE713 }, { L"help", 0xE897 },
};
const int kGlyphCount = (int)(sizeof(kGlyphs) / sizeof(kGlyphs[0]));

wchar_t GlyphChar(const std::wstring& key) {
    for (int i = 0; i < kGlyphCount; i++)
        if (key == kGlyphs[i].key) return kGlyphs[i].ch;
    return 0xECA5;   // app grid
}

const uint32_t kSwatches[] = {
    0x2D8CFF, 0x5B5FC7, 0x00A862, 0x1D9BC9, 0x0F78D4, 0x8E5CF7,
    0xE8A317, 0xD35230, 0xDB4437, 0xE0457B, 0x30B35A, 0x7D7D86,
};
const int kSwatchCount = (int)(sizeof(kSwatches) / sizeof(kSwatches[0]));

const uint32_t kAccents[] = { 0x0A84FF, 0x00B7C3, 0x30D158, 0xFF9F0A, 0xFF375F, 0xBF5AF2, 0x5E5CE6, 0x8E8E93 };
const int kAccentCount = (int)(sizeof(kAccents) / sizeof(kAccents[0]));

const WidgetType kWidgetTypes[] = {
    { L"analog",     L"Analog clock",  L"A clock face with hour, minute and second hands." },
    { L"digital",    L"Digital clock", L"Big hours and minutes, with seconds, a greeting and the date." },
    { L"calendar",   L"Calendar",      L"This month, with today highlighted. Browse with the arrows." },
    { L"worldclock", L"World clock",   L"The time in another city, for calls across time zones." },
    { L"note",       L"Note",          L"A message for everyone in the room: guest Wi-Fi, house rules, who to call." },
    { L"photo",      L"Photo",         L"One picture, or a folder of pictures shown as a slideshow." },
};
const int kWidgetTypeCount = (int)(sizeof(kWidgetTypes) / sizeof(kWidgetTypes[0]));

const uint32_t kNoteColors[] = { 0, 0xFFD60A, 0xFF9F0A, 0xFF375F, 0xBF5AF2, 0x0A84FF, 0x30D158 };
const int kNoteColorCount = (int)(sizeof(kNoteColors) / sizeof(kNoteColors[0]));

const WidgetType* WidgetTypeOf(const std::wstring& type) {
    for (int i = 0; i < kWidgetTypeCount; i++)
        if (type == kWidgetTypes[i].type) return &kWidgetTypes[i];
    return nullptr;
}

WidgetCfg NewWidget(const std::wstring& type) {
    WidgetCfg w;
    w.uid = NewUid();
    w.type = type;
    if (type == L"worldclock") {   // new widgets start in the current UI language
        w.city = Tr(L"London");
        w.zone = L"GMT Standard Time";
        w.seconds = false;
    } else if (type == L"note") {
        w.title = Tr(L"Welcome");
        w.text = Tr(L"Please leave the room as you found it.\r\nNeed help? Call IT on ext. 1234.");
        w.color = 0xFFD60A;
    }
    return w;
}

uint32_t NewUid() {
    static uint32_t next = 1;
    return next++;
}

DockItem ItemFromPreset(const Preset& p) {
    DockItem d;
    d.uid = NewUid();
    d.name = p.name;
    d.kind = p.kind;
    d.target = p.target;
    d.glyph = p.glyph;
    d.color = p.color;
    return d;
}

const Preset* PresetForKind(const std::wstring& kind) {
    for (int i = 0; i < kPresetCount; i++)
        if (kind == kPresets[i].kind && !IsCustomKind(kind)) return &kPresets[i];
    return nullptr;
}

bool IsCustomKind(const std::wstring& kind) { return kind == L"web" || kind == L"program"; }

void ConfigDefaults(Config& c) {
    c = Config();
    const int order[] = { 0, 1, 2, 3, 4, 6 };   // Zoom, Teams, Meet, Webex, Edge, File Explorer
    for (int k : order) c.dock.push_back(ItemFromPreset(kPresets[k]));
    for (const wchar_t* t : { L"analog", L"digital", L"calendar" }) c.widgets.push_back(NewWidget(t));
}

static std::wstring s_configOverride;

std::wstring ConfigPath() { return s_configOverride.empty() ? AppDataDir() + L"\\config.json" : s_configOverride; }

static uint32_t ParseColor(const std::wstring& s, uint32_t def) {
    std::wstring t = Trim(s);
    if (!t.empty() && t[0] == L'#') t = t.substr(1);
    if (t.size() != 6) return def;
    wchar_t* end = nullptr;
    unsigned long v = wcstoul(t.c_str(), &end, 16);
    return end && *end == 0 ? (uint32_t)v : def;
}

static std::wstring ColorStr(uint32_t c) { return Format(L"#%06X", c & 0xFFFFFF); }

static bool LoadFrom(const std::wstring& path, Config& c) {
    std::string bytes;
    if (!ReadFileBytes(path, bytes)) return false;
    JValue root;
    if (!JsonParse(bytes, root) || root.type != JValue::Object) return false;

    Config d;   // defaults for missing keys
    c.roomName = Trim(root.GetStr(L"roomName", d.roomName));
    // 1.0/1.1 stored the default name itself; empty now means "the default, in the UI language"
    if (c.roomName == L"Meeting Room") c.roomName.clear();
    c.subtitle = root.GetStr(L"subtitle", L"");
    std::wstring clock = root.GetStr(L"clock", L"auto");
    c.clockMode = clock == L"24h" ? 1 : clock == L"12h" ? 2 : 0;
    std::wstring week = root.GetStr(L"weekStart", L"auto");
    c.weekStart = week == L"sunday" ? 1 : week == L"monday" ? 2 : 0;
    std::wstring theme = root.GetStr(L"theme", L"auto");
    c.theme = theme == L"dark" ? 1 : theme == L"light" ? 2 : 0;
    std::wstring lang = ToLower(root.GetStr(L"language", L"auto"));
    c.language = (lang == L"en" || lang == L"english") ? 1 : (lang == L"th" || lang == L"thai") ? 2 : 0;
    c.accent = ParseColor(root.GetStr(L"accent", L""), d.accent);
    c.wallpaperDark = root.GetStr(L"wallpaperDark", L"");
    c.wallpaperLight = root.GetStr(L"wallpaperLight", L"");
    c.keepAwake = root.GetBool(L"keepScreenOn", false);
    c.idleMinutes = (int)Clamp((float)root.GetNum(L"ambientAfterMinutes", d.idleMinutes), 0, 240);
    c.dockCapacity = (int)Clamp((float)root.GetNum(L"dockSlots", 6), 3, 8);
    c.volumeStep = (int)Clamp((float)root.GetNum(L"volumeStep", 5), 1, 20);
    c.adminPin = root.GetStr(L"adminPin", L"");
    c.monitor = (int)Clamp((float)root.GetNum(L"monitor", 0), 0, 16);

    c.dock.clear();
    if (const JValue* dock = root.Find(L"dock"); dock && dock->type == JValue::Array) {
        for (auto& e : dock->arr) {
            if (e.type != JValue::Object) continue;
            DockItem it;
            it.uid = NewUid();
            it.name = e.GetStr(L"name", L"App");
            it.kind = ToLower(e.GetStr(L"kind", L"program"));
            it.target = e.GetStr(L"target", L"");
            it.args = e.GetStr(L"args", L"");
            it.glyph = e.GetStr(L"icon", L"apps");
            it.color = ParseColor(e.GetStr(L"color", L""), 0x3D7BFF);
            c.dock.push_back(it);
            if ((int)c.dock.size() >= c.dockCapacity) break;
        }
    } else {
        Config def;
        ConfigDefaults(def);
        c.dock = def.dock;
    }

    c.widgets.clear();
    if (const JValue* wl = root.Find(L"widgets"); wl && wl->type == JValue::Array) {
        for (auto& e : wl->arr) {
            if (e.type != JValue::Object) continue;
            std::wstring t = ToLower(e.GetStr(L"type", L""));
            if (!WidgetTypeOf(t)) continue;
            WidgetCfg w = NewWidget(t);
            w.seconds = e.GetBool(L"seconds", w.seconds);
            w.numbers = e.GetBool(L"numbers", w.numbers);
            w.date = e.GetBool(L"date", w.date);
            w.greeting = e.GetBool(L"greeting", w.greeting);
            w.weekNumbers = e.GetBool(L"weekNumbers", w.weekNumbers);
            w.large = ToLower(e.GetStr(L"size", w.large ? L"large" : L"normal")) == L"large";
            w.analog = ToLower(e.GetStr(L"style", w.analog ? L"analog" : L"digital")) == L"analog";
            w.city = e.GetStr(L"city", w.city);
            w.zone = e.GetStr(L"zone", w.zone);
            w.title = e.GetStr(L"title", w.title);
            w.text = e.GetStr(L"text", w.text);
            std::wstring col = e.GetStr(L"color", L"-");
            if (col != L"-") w.color = Trim(col).empty() ? 0 : ParseColor(col, 0);
            w.path = e.GetStr(L"path", w.path);
            w.folder = ToLower(e.GetStr(L"source", w.folder ? L"folder" : L"photo")) == L"folder";
            w.interval = (int)Clamp((float)e.GetNum(L"interval", w.interval), 5, 3600);
            w.shuffle = e.GetBool(L"shuffle", w.shuffle);
            std::wstring shape = ToLower(e.GetStr(L"shape", L"landscape"));
            w.shape = shape == L"square" ? 1 : shape == L"portrait" ? 2 : 0;
            w.fit = ToLower(e.GetStr(L"fit", w.fit ? L"fit" : L"fill")) == L"fit";
            w.caption = e.GetStr(L"caption", w.caption);
            c.widgets.push_back(w);
            if ((int)c.widgets.size() >= kMaxWidgets) break;
        }
    } else {   // settings from 1.0.0: the original three
        for (const wchar_t* t : { L"analog", L"digital", L"calendar" }) c.widgets.push_back(NewWidget(t));
    }
    return true;
}

bool ConfigLoad(Config& c, const std::wstring& pathOverride) {
    ConfigDefaults(c);
    if (!pathOverride.empty()) {
        s_configOverride = pathOverride;
        if (!FileExists(pathOverride)) return true;   // created on first save
        return LoadFrom(pathOverride, c);
    }
    if (LoadFrom(ConfigPath(), c)) return true;
    // IT can ship defaults beside the exe.
    return LoadFrom(ExeDir() + L"\\RoomHome.defaults.json", c);
}

bool ConfigSave(const Config& c) {
    JValue root = JValue::MakeObject();
    root.Set(L"roomName", JValue::Str(c.roomName));
    root.Set(L"subtitle", JValue::Str(c.subtitle));
    root.Set(L"clock", JValue::Str(c.clockMode == 1 ? L"24h" : c.clockMode == 2 ? L"12h" : L"auto"));
    root.Set(L"weekStart", JValue::Str(c.weekStart == 1 ? L"sunday" : c.weekStart == 2 ? L"monday" : L"auto"));
    root.Set(L"theme", JValue::Str(c.theme == 1 ? L"dark" : c.theme == 2 ? L"light" : L"auto"));
    root.Set(L"language", JValue::Str(c.language == 1 ? L"en" : c.language == 2 ? L"th" : L"auto"));
    root.Set(L"accent", JValue::Str(ColorStr(c.accent)));
    root.Set(L"wallpaperDark", JValue::Str(c.wallpaperDark));
    root.Set(L"wallpaperLight", JValue::Str(c.wallpaperLight));
    root.Set(L"keepScreenOn", JValue::Boolean(c.keepAwake));
    root.Set(L"ambientAfterMinutes", JValue::Num(c.idleMinutes));
    root.Set(L"dockSlots", JValue::Num(c.dockCapacity));
    root.Set(L"volumeStep", JValue::Num(c.volumeStep));
    root.Set(L"adminPin", JValue::Str(c.adminPin));
    root.Set(L"monitor", JValue::Num(c.monitor));
    JValue dock = JValue::MakeArray();
    for (auto& it : c.dock) {
        JValue e = JValue::MakeObject();
        e.Set(L"name", JValue::Str(it.name));
        e.Set(L"kind", JValue::Str(it.kind));
        e.Set(L"target", JValue::Str(it.target));
        e.Set(L"args", JValue::Str(it.args));
        e.Set(L"icon", JValue::Str(it.glyph));
        e.Set(L"color", JValue::Str(ColorStr(it.color)));
        dock.arr.push_back(e);
    }
    root.Set(L"dock", dock);
    JValue widgets = JValue::MakeArray();
    for (auto& w : c.widgets) {
        JValue e = JValue::MakeObject();
        e.Set(L"type", JValue::Str(w.type));
        if (w.type == L"analog") {
            e.Set(L"seconds", JValue::Boolean(w.seconds));
            e.Set(L"numbers", JValue::Boolean(w.numbers));
            e.Set(L"date", JValue::Boolean(w.date));
        } else if (w.type == L"digital") {
            e.Set(L"seconds", JValue::Boolean(w.seconds));
            e.Set(L"greeting", JValue::Boolean(w.greeting));
            e.Set(L"date", JValue::Boolean(w.date));
            e.Set(L"size", JValue::Str(w.large ? L"large" : L"normal"));
        } else if (w.type == L"calendar") {
            e.Set(L"weekNumbers", JValue::Boolean(w.weekNumbers));
        } else if (w.type == L"worldclock") {
            e.Set(L"city", JValue::Str(w.city));
            e.Set(L"zone", JValue::Str(w.zone));
            e.Set(L"style", JValue::Str(w.analog ? L"analog" : L"digital"));
            e.Set(L"seconds", JValue::Boolean(w.seconds));
        } else if (w.type == L"note") {
            e.Set(L"title", JValue::Str(w.title));
            e.Set(L"text", JValue::Str(w.text));
            e.Set(L"size", JValue::Str(w.large ? L"large" : L"normal"));
            e.Set(L"color", JValue::Str(w.color ? ColorStr(w.color) : L""));
        } else if (w.type == L"photo") {
            e.Set(L"path", JValue::Str(w.path));
            e.Set(L"source", JValue::Str(w.folder ? L"folder" : L"photo"));
            e.Set(L"interval", JValue::Num(w.interval));
            e.Set(L"shuffle", JValue::Boolean(w.shuffle));
            e.Set(L"shape", JValue::Str(w.shape == 1 ? L"square" : w.shape == 2 ? L"portrait" : L"landscape"));
            e.Set(L"fit", JValue::Str(w.fit ? L"fit" : L"fill"));
            e.Set(L"caption", JValue::Str(w.caption));
        }
        widgets.arr.push_back(e);
    }
    root.Set(L"widgets", widgets);
    return WriteFileAtomic(ConfigPath(), JsonWrite(root));
}
