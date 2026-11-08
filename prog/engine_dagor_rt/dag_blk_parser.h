//
// Dagor Engine 6.5 Runtime — Native .blk (DataBlock) Text Parser & Serializer
// Supports Gaijin DataBlock syntax (:t=, :i=, :r=, :b=, :p3=, :p4=, :ip3=, nested blocks & comments)
//
#pragma once
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#ifdef _WIN32
#ifndef _TARGET_PC_WIN
#define _TARGET_PC_WIN 1
#endif
#else
#ifndef _TARGET_PC_LINUX
#define _TARGET_PC_LINUX 1
#endif
#endif
#ifndef _TARGET_64BIT
#define _TARGET_64BIT 1
#endif
#ifndef _TARGET_SIMD_SSE
#define _TARGET_SIMD_SSE 4
#endif
#include <supp/dag_define_KRNLIMP.h>
#include <math/dag_Point3.h>
#include <math/dag_Point4.h>
#include <math/dag_TMatrix.h>
#include <math/dag_e3dColor.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <cctype>

namespace dagor {

struct BlkIPoint3 {
    int x = 0, y = 0, z = 0;
};

struct BlkParam {
    std::string name;
    std::string typeTag; // t, i, r, b, p3, p4, ip3
    std::string rawVal;
};

class DataBlockLite {
public:
    std::string blockName;
    std::vector<BlkParam> params;
    std::vector<DataBlockLite> blocks;

    void clear() {
        blockName.clear();
        params.clear();
        blocks.clear();
    }

    const BlkParam* findParam(const char* name) const {
        for (const auto& p : params) {
            if (p.name == name) return &p;
        }
        return nullptr;
    }

    const DataBlockLite* getBlockByName(const char* name) const {
        for (const auto& b : blocks) {
            if (b.blockName == name) return &b;
        }
        return nullptr;
    }

    int blockCount() const { return (int)blocks.size(); }
    const DataBlockLite& getBlock(int idx) const { return blocks[idx]; }

    std::string getStr(const char* name, const char* def = "") const {
        const auto* p = findParam(name);
        return p ? p->rawVal : std::string(def ? def : "");
    }

    int getInt(const char* name, int def = 0) const {
        const auto* p = findParam(name);
        if (!p) return def;
        return std::atoi(p->rawVal.c_str());
    }

    float getReal(const char* name, float def = 0.f) const {
        const auto* p = findParam(name);
        if (!p) return def;
        return (float)std::strtod(p->rawVal.c_str(), nullptr);
    }

    bool getBool(const char* name, bool def = false) const {
        const auto* p = findParam(name);
        if (!p) return def;
        const std::string& v = p->rawVal;
        if (v == "yes" || v == "true" || v == "1" || v == "on") return true;
        if (v == "no"  || v == "false"|| v == "0" || v == "off") return false;
        return def;
    }

    Point3 getPoint3(const char* name, const Point3& def = Point3(0.f, 0.f, 0.f)) const {
        const auto* p = findParam(name);
        if (!p) return def;
        float x = def.x, y = def.y, z = def.z;
        if (std::sscanf(p->rawVal.c_str(), "%f , %f , %f", &x, &y, &z) == 3)
            return Point3(x, y, z);
        return def;
    }

    BlkIPoint3 getIPoint3(const char* name, BlkIPoint3 def = {0, 0, 0}) const {
        const auto* p = findParam(name);
        if (!p) return def;
        int x = def.x, y = def.y, z = def.z;
        if (std::sscanf(p->rawVal.c_str(), "%d , %d , %d", &x, &y, &z) == 3)
            return {x, y, z};
        return def;
    }

    bool loadFromText(const std::string& src) {
        clear();
        size_t pos = 0;
        parseBody(src, pos, false);
        return !params.empty() || !blocks.empty();
    }

private:
    static void skipWsAndComments(const std::string& s, size_t& i) {
        const size_t n = s.size();
        while (i < n) {
            unsigned char c = (unsigned char)s[i];
            if (std::isspace(c) || c == ';') {
                ++i;
            } else if (i + 1 < n && s[i] == '/' && s[i + 1] == '/') {
                i += 2;
                while (i < n && s[i] != '\n') ++i;
            } else if (i + 1 < n && s[i] == '/' && s[i + 1] == '*') {
                i += 2;
                while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) ++i;
                if (i + 1 < n) i += 2;
            } else {
                break;
            }
        }
    }

    static std::string readIdentOrQuoted(const std::string& s, size_t& i) {
        skipWsAndComments(s, i);
        const size_t n = s.size();
        if (i >= n) return "";
        if (s[i] == '"') {
            ++i;
            std::string out;
            while (i < n && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < n) { ++i; out.push_back(s[i++]); }
                else out.push_back(s[i++]);
            }
            if (i < n && s[i] == '"') ++i;
            return out;
        }
        size_t start = i;
        while (i < n) {
            unsigned char c = (unsigned char)s[i];
            if (std::isspace(c) || c == '{' || c == '}' || c == ':' || c == '=' || c == ';') break;
            if (i + 1 < n && s[i] == '/' && (s[i + 1] == '/' || s[i + 1] == '*')) break;
            ++i;
        }
        return s.substr(start, i - start);
    }

    static std::string trimStr(const std::string& v) {
        size_t a = 0, b = v.size();
        while (a < b && std::isspace((unsigned char)v[a])) ++a;
        while (b > a && std::isspace((unsigned char)v[b - 1])) --b;
        return v.substr(a, b - a);
    }

    void parseBody(const std::string& s, size_t& i, bool stopAtBrace) {
        const size_t n = s.size();
        while (i < n) {
            skipWsAndComments(s, i);
            if (i >= n) break;
            if (s[i] == '}') {
                if (stopAtBrace) { ++i; return; }
                ++i;
                continue;
            }

            std::string key = readIdentOrQuoted(s, i);
            if (key.empty()) { ++i; continue; }

            skipWsAndComments(s, i);
            if (i < n && s[i] == '{') {
                ++i;
                DataBlockLite child;
                child.blockName = key;
                child.parseBody(s, i, true);
                blocks.push_back(std::move(child));
            } else if (i < n && s[i] == ':') {
                ++i;
                std::string ttag = readIdentOrQuoted(s, i);
                skipWsAndComments(s, i);
                if (i < n && s[i] == '=') ++i;
                skipWsAndComments(s, i);

                std::string val;
                if (i < n && s[i] == '"') {
                    val = readIdentOrQuoted(s, i);
                } else {
                    size_t start = i;
                    while (i < n && s[i] != '\n' && s[i] != '\r' && s[i] != ';' && s[i] != '}') {
                        if (i + 1 < n && s[i] == '/' && (s[i + 1] == '/' || s[i + 1] == '*')) break;
                        ++i;
                    }
                    val = trimStr(s.substr(start, i - start));
                }
                params.push_back({key, ttag, val});
            } else if (i < n && s[i] == '=') {
                ++i;
                skipWsAndComments(s, i);
                std::string val = (i < n && s[i] == '"') ? readIdentOrQuoted(s, i) : "";
                params.push_back({key, "t", val});
            }
        }
    }
};

} // namespace dagor
