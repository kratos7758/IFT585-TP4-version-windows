#pragma once
// =============================================================
//  json.h  –  Implémentation JSON minimale header-only
//  IFT585 – TP4   (aucune dépendance externe)
// =============================================================
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cctype>

class Json {
public:
    enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT };

    // ---- Constructeurs ----
    Json() : type_(NUL), num_(0), bool_(false) {}
    explicit Json(bool b)              : type_(BOOL),   num_(0),   bool_(b)  {}
    explicit Json(int n)               : type_(NUMBER), num_(n),   bool_(false) {}
    explicit Json(long long n)         : type_(NUMBER), num_((double)n), bool_(false) {}
    explicit Json(double n)            : type_(NUMBER), num_(n),   bool_(false) {}
    explicit Json(const std::string& s): type_(STRING), num_(0),   bool_(false), str_(s) {}
    explicit Json(const char* s)       : type_(STRING), num_(0),   bool_(false), str_(s) {}

    static Json object() { Json j; j.type_ = OBJECT; return j; }
    static Json array()  { Json j; j.type_ = ARRAY;  return j; }

    // ---- Vérification de type ----
    bool is_null()   const { return type_ == NUL;    }
    bool is_bool()   const { return type_ == BOOL;   }
    bool is_number() const { return type_ == NUMBER; }
    bool is_string() const { return type_ == STRING; }
    bool is_array()  const { return type_ == ARRAY;  }
    bool is_object() const { return type_ == OBJECT; }

    // ---- Accesseurs de valeur ----
    bool               get_bool()   const { return bool_; }
    double             get_double() const { return num_;  }
    int                get_int()    const { return (int)num_; }
    long long          get_ll()     const { return (long long)num_; }
    const std::string& get_string() const { return str_; }

    const std::vector<Json>& get_array()  const { return arr_; }
    std::vector<Json>&       get_array()        { return arr_; }
    const std::map<std::string,Json>& get_object() const { return obj_; }
    std::map<std::string,Json>&       get_object()       { return obj_; }

    // ---- Accès objet ----
    Json& operator[](const std::string& key) {
        if (type_ != OBJECT) type_ = OBJECT;
        return obj_[key];
    }
    const Json& at(const std::string& key) const { return obj_.at(key); }
    bool contains(const std::string& key) const {
        return type_ == OBJECT && obj_.count(key) > 0;
    }

    // ---- Accès tableau ----
    Json&       operator[](size_t i)       { return arr_[i]; }
    const Json& operator[](size_t i) const { return arr_[i]; }
    size_t size() const {
        if (type_ == ARRAY)  return arr_.size();
        if (type_ == OBJECT) return obj_.size();
        return 0;
    }
    bool empty() const { return size() == 0; }
    void push_back(const Json& v) {
        if (type_ != ARRAY) type_ = ARRAY;
        arr_.push_back(v);
    }

    // ---- Sérialisation ----
    std::string dump(int indent = -1, int depth = 0) const {
        std::string tab (indent < 0 ? 0 : (size_t)(depth + 1) * (size_t)indent, ' ');
        std::string tab0(indent < 0 ? 0 : (size_t) depth      * (size_t)indent, ' ');
        std::string nl = indent < 0 ? "" : "\n";
        std::string sp = indent < 0 ? "" : " ";
        switch (type_) {
        case NUL:    return "null";
        case BOOL:   return bool_ ? "true" : "false";
        case NUMBER: {
            char buf[64];
            if (num_ == (long long)num_)
                snprintf(buf, sizeof(buf), "%lld", (long long)num_);
            else
                snprintf(buf, sizeof(buf), "%g", num_);
            return buf;
        }
        case STRING: return escape(str_);
        case ARRAY: {
            if (arr_.empty()) return "[]";
            std::string s = "[" + nl;
            for (size_t i = 0; i < arr_.size(); i++) {
                s += tab + arr_[i].dump(indent, depth + 1);
                if (i + 1 < arr_.size()) s += ",";
                s += nl;
            }
            return s + tab0 + "]";
        }
        case OBJECT: {
            if (obj_.empty()) return "{}";
            std::string s = "{" + nl;
            size_t i = 0;
            for (const auto& kv : obj_) {
                s += tab + escape(kv.first) + ":" + sp + kv.second.dump(indent, depth + 1);
                if (++i < obj_.size()) s += ",";
                s += nl;
            }
            return s + tab0 + "}";
        }
        }
        return "null";
    }

    // ---- Désérialisation ----
    static Json parse(const std::string& s) {
        size_t pos = 0;
        skipWS(s, pos);
        return parseValue(s, pos);
    }

private:
    Type   type_;
    double num_;
    bool   bool_;
    std::string str_;
    std::vector<Json> arr_;
    std::map<std::string, Json> obj_;

    static std::string escape(const std::string& s) {
        std::string r = "\"";
        for (unsigned char c : s) {
            if      (c == '"')  r += "\\\"";
            else if (c == '\\') r += "\\\\";
            else if (c == '\n') r += "\\n";
            else if (c == '\r') r += "\\r";
            else if (c == '\t') r += "\\t";
            else                r += (char)c;
        }
        return r + "\"";
    }

    static void skipWS(const std::string& s, size_t& pos) {
        while (pos < s.size() && isspace((unsigned char)s[pos])) pos++;
    }

    static Json parseValue(const std::string& s, size_t& pos) {
        skipWS(s, pos);
        if (pos >= s.size()) return Json();
        char c = s[pos];
        if (c == '"') return parseString(s, pos);
        if (c == '{') return parseObject(s, pos);
        if (c == '[') return parseArray(s, pos);
        if (c == 't') { pos += 4; return Json(true);  }
        if (c == 'f') { pos += 5; return Json(false); }
        if (c == 'n') { pos += 4; return Json();      }
        return parseNumber(s, pos);
    }

    static Json parseString(const std::string& s, size_t& pos) {
        pos++; // skip opening "
        std::string r;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                pos++;
                char e = s[pos];
                if      (e == '"')  r += '"';
                else if (e == '\\') r += '\\';
                else if (e == 'n')  r += '\n';
                else if (e == 'r')  r += '\r';
                else if (e == 't')  r += '\t';
                else                r += e;
            } else {
                r += s[pos];
            }
            pos++;
        }
        if (pos < s.size()) pos++; // skip closing "
        return Json(r);
    }

    static Json parseNumber(const std::string& s, size_t& pos) {
        size_t start = pos;
        if (pos < s.size() && s[pos] == '-') pos++;
        while (pos < s.size() && (isdigit((unsigned char)s[pos]) ||
               s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' ||
               s[pos] == '+' || s[pos] == '-'))
            pos++;
        return Json(std::stod(s.substr(start, pos - start)));
    }

    static Json parseObject(const std::string& s, size_t& pos) {
        pos++; // skip {
        Json j = Json::object();
        skipWS(s, pos);
        if (pos < s.size() && s[pos] == '}') { pos++; return j; }
        while (pos < s.size()) {
            skipWS(s, pos);
            if (s[pos] != '"') break;
            std::string key = parseString(s, pos).get_string();
            skipWS(s, pos);
            if (pos < s.size() && s[pos] == ':') pos++;
            skipWS(s, pos);
            j[key] = parseValue(s, pos);
            skipWS(s, pos);
            if (pos < s.size() && s[pos] == ',') { pos++; continue; }
            if (pos < s.size() && s[pos] == '}') { pos++; break; }
        }
        return j;
    }

    static Json parseArray(const std::string& s, size_t& pos) {
        pos++; // skip [
        Json j = Json::array();
        skipWS(s, pos);
        if (pos < s.size() && s[pos] == ']') { pos++; return j; }
        while (pos < s.size()) {
            skipWS(s, pos);
            j.push_back(parseValue(s, pos));
            skipWS(s, pos);
            if (pos < s.size() && s[pos] == ',') { pos++; continue; }
            if (pos < s.size() && s[pos] == ']') { pos++; break; }
        }
        return j;
    }
};
