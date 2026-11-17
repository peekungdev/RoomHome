#include "common.h"
#include "json.h"

const JValue* JValue::Find(const wchar_t* key) const {
    if (type != Object) return nullptr;
    for (auto& kv : obj)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

void JValue::Set(const std::wstring& key, const JValue& v) {
    type = Object;
    for (auto& kv : obj)
        if (kv.first == key) { kv.second = v; return; }
    obj.emplace_back(key, v);
}

std::wstring JValue::GetStr(const wchar_t* key, const std::wstring& def) const {
    const JValue* v = Find(key);
    return v && v->type == String ? v->s : def;
}

double JValue::GetNum(const wchar_t* key, double def) const {
    const JValue* v = Find(key);
    if (!v) return def;
    if (v->type == Number) return v->n;
    if (v->type == Bool) return v->b ? 1 : 0;
    return def;
}

bool JValue::GetBool(const wchar_t* key, bool def) const {
    const JValue* v = Find(key);
    if (!v) return def;
    if (v->type == Bool) return v->b;
    if (v->type == Number) return v->n != 0;
    return def;
}

namespace {

struct Parser {
    const std::string& t;
    size_t i = 0;
    int depth = 0;
    explicit Parser(const std::string& s) : t(s) {}

    void Ws() {
        while (i < t.size()) {
            char c = t[i];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { i++; continue; }
            if (c == '/' && i + 1 < t.size() && t[i + 1] == '/') {   // tolerate // comments
                while (i < t.size() && t[i] != '\n') i++;
                continue;
            }
            break;
        }
    }

    static void PutUtf8(std::string& o, unsigned cp) {
        if (cp < 0x80) o += (char)cp;
        else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F));
        } else {
            o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F));
            o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F));
        }
    }

    bool Hex4(unsigned& v) {
        if (i + 4 > t.size()) return false;
        v = 0;
        for (int k = 0; k < 4; k++) {
            char c = t[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return false;
        }
        return true;
    }

    bool Str(std::wstring& out) {
        if (i >= t.size() || t[i] != '"') return false;
        i++;
        std::string o;
        while (i < t.size()) {
            char c = t[i++];
            if (c == '"') { out = Utf8ToW(o); return true; }
            if (c != '\\') { o += c; continue; }
            if (i >= t.size()) return false;
            char e = t[i++];
            switch (e) {
            case '"': o += '"'; break;
            case '\\': o += '\\'; break;
            case '/': o += '/'; break;
            case 'b': o += '\b'; break;
            case 'f': o += '\f'; break;
            case 'n': o += '\n'; break;
            case 'r': o += '\r'; break;
            case 't': o += '\t'; break;
            case 'u': {
                unsigned cp;
                if (!Hex4(cp)) return false;
                if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < t.size() && t[i] == '\\' && t[i + 1] == 'u') {
                    size_t save = i;
                    i += 2;
                    unsigned lo;
                    if (Hex4(lo) && lo >= 0xDC00 && lo < 0xE000) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    else i = save;
                }
                PutUtf8(o, cp);
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    bool Val(JValue& v) {
        if (++depth > 64) return false;
        Ws();
        if (i >= t.size()) return false;
        char c = t[i];
        bool ok = false;
        if (c == '{') {
            i++;
            v = JValue::MakeObject();
            Ws();
            if (i < t.size() && t[i] == '}') { i++; ok = true; }
            else for (;;) {
                Ws();
                std::wstring k;
                if (!Str(k)) break;
                Ws();
                if (i >= t.size() || t[i] != ':') break;
                i++;
                JValue child;
                if (!Val(child)) break;
                v.obj.emplace_back(std::move(k), std::move(child));
                Ws();
                if (i < t.size() && t[i] == ',') {
                    i++; Ws();
                    if (i < t.size() && t[i] == '}') { i++; ok = true; break; }   // trailing comma
                    continue;
                }
                if (i < t.size() && t[i] == '}') { i++; ok = true; }
                break;
            }
        } else if (c == '[') {
            i++;
            v = JValue::MakeArray();
            Ws();
            if (i < t.size() && t[i] == ']') { i++; ok = true; }
            else for (;;) {
                JValue child;
                if (!Val(child)) break;
                v.arr.push_back(std::move(child));
                Ws();
                if (i < t.size() && t[i] == ',') {
                    i++; Ws();
                    if (i < t.size() && t[i] == ']') { i++; ok = true; break; }
                    continue;
                }
                if (i < t.size() && t[i] == ']') { i++; ok = true; }
                break;
            }
        } else if (c == '"') {
            v.type = JValue::String;
            ok = Str(v.s);
        } else if (t.compare(i, 4, "true") == 0) { i += 4; v = JValue::Boolean(true); ok = true; }
        else if (t.compare(i, 5, "false") == 0) { i += 5; v = JValue::Boolean(false); ok = true; }
        else if (t.compare(i, 4, "null") == 0) { i += 4; v = JValue(); ok = true; }
        else if (c == '-' || (c >= '0' && c <= '9')) {
            size_t st = i;
            while (i < t.size() && strchr("+-0123456789.eE", t[i])) i++;
            std::string num = t.substr(st, i - st);
            char* end = nullptr;
            double d = strtod(num.c_str(), &end);
            ok = end && *end == 0;
            v = JValue::Num(d);
        }
        depth--;
        return ok;
    }
};

void Esc(std::string& o, const std::wstring& ws) {
    std::string s = WToUtf8(ws);
    o += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break;
        case '\r': o += "\\r"; break;
        case '\t': o += "\\t"; break;
        default:
            if (c < 0x20) { char b[8]; sprintf_s(b, "\\u%04x", c); o += b; }
            else o += (char)c;
        }
    }
    o += '"';
}

void Write(std::string& o, const JValue& v, int ind) {
    std::string pad(ind * 2, ' '), pad2((ind + 1) * 2, ' ');
    switch (v.type) {
    case JValue::Null: o += "null"; break;
    case JValue::Bool: o += v.b ? "true" : "false"; break;
    case JValue::Number: {
        char b[64];
        if (std::floor(v.n) == v.n && std::fabs(v.n) < 1e15) sprintf_s(b, "%lld", (long long)v.n);
        else sprintf_s(b, "%.10g", v.n);
        o += b;
        break;
    }
    case JValue::String: Esc(o, v.s); break;
    case JValue::Array:
        if (v.arr.empty()) { o += "[]"; break; }
        o += "[\n";
        for (size_t k = 0; k < v.arr.size(); k++) {
            o += pad2;
            Write(o, v.arr[k], ind + 1);
            o += k + 1 < v.arr.size() ? ",\n" : "\n";
        }
        o += pad + "]";
        break;
    case JValue::Object:
        if (v.obj.empty()) { o += "{}"; break; }
        o += "{\n";
        for (size_t k = 0; k < v.obj.size(); k++) {
            o += pad2;
            Esc(o, v.obj[k].first);
            o += ": ";
            Write(o, v.obj[k].second, ind + 1);
            o += k + 1 < v.obj.size() ? ",\n" : "\n";
        }
        o += pad + "}";
        break;
    }
}

} // namespace

bool JsonParse(const std::string& utf8, JValue& out) {
    std::string text = utf8;
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        text.erase(0, 3);
    Parser p(text);
    if (!p.Val(out)) return false;
    p.Ws();
    return p.i == text.size();
}

std::string JsonWrite(const JValue& v) {
    std::string o;
    Write(o, v, 0);
    o += "\n";
    return o;
}
