//
// Dagor Engine 6.5 Runtime — Upstream Gaijin Quirrel (.nut) Virtual Machine Host
// Powered by GaijinEntertainment/DagorEngine prog/1stPartyLibs/quirrel
//
#pragma once

#include <squirrel.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdaux.h>
#include <string>
#include <vector>
#include <cstdarg>
#include <cstdio>

namespace dagor {

class QuirrelVmHost {
public:
    HSQUIRRELVM vm = nullptr;
    bool active = false;
    std::string bootBanner = "Quirrel VM (.nut)";
    std::vector<std::string> vmLogs;

    static void printFunc(HSQUIRRELVM v, const char* fmt, ...) {
        char buf[512];
        va_list vl;
        va_start(vl, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, vl);
        va_end(vl);
        if (auto* self = reinterpret_cast<QuirrelVmHost*>(sq_getforeignptr(v))) {
            self->pushLog(std::string("[quirrel] ") + buf);
        }
    }

    static void errorFunc(HSQUIRRELVM v, const char* fmt, ...) {
        char buf[512];
        va_list vl;
        va_start(vl, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, vl);
        va_end(vl);
        if (auto* self = reinterpret_cast<QuirrelVmHost*>(sq_getforeignptr(v))) {
            self->pushLog(std::string("[quirrel:err] ") + buf);
        }
    }

    void pushLog(const std::string& s) {
        if (vmLogs.size() > 64) vmLogs.erase(vmLogs.begin());
        vmLogs.push_back(s);
    }

    bool init(const std::string& bootNutSource) {
        shutdown();
        vm = sq_open(1024);
        if (!vm) return false;
        sq_setforeignptr(vm, this);
        sq_setprintfunc(vm, &QuirrelVmHost::printFunc, &QuirrelVmHost::errorFunc);
        sqstd_seterrorhandlers(vm);

        sq_registerbaselib(vm);
        sq_registertypeslib(vm);
        sq_pushroottable(vm);
        sqstd_register_mathlib(vm);
        sqstd_register_stringlib(vm);
        sq_pop(vm, 1);

        active = true;
        if (!bootNutSource.empty()) {
            std::string res = evalNut(bootNutSource.c_str(), "scripts/ui/dagor_rt_boot.nut");
            if (!res.empty()) bootBanner = res;
        }
        pushLog("[DAGOR QUIRREL] VM initialized -> " + bootBanner);
        return true;
    }

    std::string evalNut(const char* code, const char* srcName = "console.nut") {
        if (!vm || !code) return "VM inactive";
        SQInteger top = sq_gettop(vm);
        if (SQ_FAILED(sq_compile(vm, code, (SQInteger)std::strlen(code), srcName, SQTrue, nullptr))) {
            sq_settop(vm, top);
            return "Compile error in .nut";
        }
        sq_pushroottable(vm);
        std::string outStr;
        if (SQ_SUCCEEDED(sq_call(vm, 1, SQTrue, SQTrue))) {
            if (SQ_SUCCEEDED(sq_tostring(vm, -1))) {
                const char* s = nullptr;
                if (SQ_SUCCEEDED(sq_getstring(vm, -1, &s)) && s) outStr = s;
                sq_pop(vm, 1);
            }
        } else {
            outStr = "Runtime error in .nut";
        }
        sq_settop(vm, top);
        return outStr;
    }

    void shutdown() {
        if (vm) {
            sq_close(vm);
            vm = nullptr;
        }
        active = false;
    }

    ~QuirrelVmHost() { shutdown(); }
};

} // namespace dagor
