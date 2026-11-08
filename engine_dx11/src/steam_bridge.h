#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <cstdio>

namespace sf {

// Динамический мост Steamworks API (steam_api64.dll)
// Работает как со Steam (оверлей Shift+Tab в DirectX 11, имя профиля Steam, Rich Presence),
// так и автономно через Лаунчер без установленного Steam!
class SteamBridge {
public:
    HMODULE dll = nullptr;
    bool initialized = false;
    std::string personaName;

    using FnInit        = bool (__cdecl*)();
    using FnShutdown    = void (__cdecl*)();
    using FnCallbacks   = void (__cdecl*)();
    using FnGetFriends  = void* (__cdecl*)();
    using FnPersonaName = const char* (__cdecl*)(void*);
    using FnRichPres    = bool (__cdecl*)(void*, const char*, const char*);

    FnInit        pInit = nullptr;
    FnShutdown    pShutdown = nullptr;
    FnCallbacks   pCallbacks = nullptr;
    FnGetFriends  pGetFriends = nullptr;
    FnPersonaName pPersonaName = nullptr;
    FnRichPres    pRichPres = nullptr;

    void init() {
        // Гарантируем наличие steam_appid.txt (480 — стандартный SDK AppID для теста Steam Overlay D3D11)
        FILE* f = std::fopen("steam_appid.txt", "rb");
        if (!f) {
            f = std::fopen("steam_appid.txt", "wb");
            if (f) { std::fprintf(f, "480\n"); std::fclose(f); }
        } else {
            std::fclose(f);
        }

        dll = ::LoadLibraryA("steam_api64.dll");
        if (!dll) dll = ::LoadLibraryA("bin\\win64\\steam_api64.dll");
        if (!dll) return;

        pInit        = (FnInit)::GetProcAddress(dll, "SteamAPI_Init");
        pShutdown    = (FnShutdown)::GetProcAddress(dll, "SteamAPI_Shutdown");
        pCallbacks   = (FnCallbacks)::GetProcAddress(dll, "SteamAPI_RunCallbacks");
        pGetFriends  = (FnGetFriends)::GetProcAddress(dll, "SteamAPI_SteamFriends_v017");
        pPersonaName = (FnPersonaName)::GetProcAddress(dll, "SteamAPI_ISteamFriends_GetPersonaName");
        pRichPres    = (FnRichPres)::GetProcAddress(dll, "SteamAPI_ISteamFriends_SetRichPresence");

        if (pInit && pInit()) {
            initialized = true;
            if (pGetFriends && pPersonaName) {
                void* fr = pGetFriends();
                if (fr) {
                    const char* n = pPersonaName(fr);
                    if (n && n[0]) personaName = n;
                }
            }
            setStatus("В ангаре • Steel Front: Online");
        }
    }

    void setStatus(const char* statusUtf8) {
        if (initialized && pGetFriends && pRichPres) {
            void* fr = pGetFriends();
            if (fr) {
                pRichPres(fr, "status", statusUtf8);
                pRichPres(fr, "steam_display", "#StatusGeneric");
            }
        }
    }

    void update() {
        if (initialized && pCallbacks) pCallbacks();
    }

    void shutdown() {
        if (initialized && pShutdown) pShutdown();
        if (dll) { ::FreeLibrary(dll); dll = nullptr; }
        initialized = false;
    }
};

} // namespace sf
