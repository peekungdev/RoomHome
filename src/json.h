// Minimal JSON (UTF-8 in/out, std::wstring strings) for the settings file.
#pragma once
#include <string>
#include <vector>
#include <utility>

struct JValue {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double n = 0;
    std::wstring s;
    std::vector<JValue> arr;
    std::vector<std::pair<std::wstring, JValue>> obj;

    static JValue MakeObject() { JValue v; v.type = Object; return v; }
    static JValue MakeArray() { JValue v; v.type = Array; return v; }
    static JValue Str(const std::wstring& x) { JValue v; v.type = String; v.s = x; return v; }
    static JValue Num(double x) { JValue v; v.type = Number; v.n = x; return v; }
    static JValue Boolean(bool x) { JValue v; v.type = Bool; v.b = x; return v; }

    const JValue* Find(const wchar_t* key) const;
    void Set(const std::wstring& key, const JValue& v);
    std::wstring GetStr(const wchar_t* key, const std::wstring& def = L"") const;
    double GetNum(const wchar_t* key, double def) const;
    bool GetBool(const wchar_t* key, bool def) const;
};

bool JsonParse(const std::string& utf8, JValue& out);
std::string JsonWrite(const JValue& v);
