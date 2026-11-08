#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "game_data.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

namespace sf {

struct GameSettings {
    int  qualityPreset = 3;   // 0=Низкое, 1=Среднее, 2=Высокое, 3=Кино (Dagor Ultra)
    int  displayMode   = 0;   // 0=Безрамочный (Весь экран), 1=В окне 1600x900, 2=В окне 1280x720
    int  msaaSamples   = 4;   // 1 или 4
    bool vsync         = true;
    bool shadows       = true;
    bool soundEnabled  = true;
    char serverIp[64]  = "127.0.0.1"; // 127.0.0.1 = автозапуск встроенного хост-сервера или подключение к локальному
    int  serverPort    = 27015;
    char callsign[24]  = "Командир";

    void load(const char* path = "config\\settings.ini") {
        FILE* f = std::fopen(path, "rb");
        if (!f) f = std::fopen("..\\..\\config\\settings.ini", "rb");
        if (!f) return;
        char line[256];
        while (std::fgets(line, sizeof(line), f)) {
            char* eq = std::strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            char* val = eq + 1;
            size_t l = std::strlen(val);
            while (l > 0 && (val[l - 1] == '\n' || val[l - 1] == '\r' || val[l - 1] == ' ')) val[--l] = 0;
            if (std::strcmp(line, "quality") == 0) qualityPreset = std::atoi(val);
            else if (std::strcmp(line, "displayMode") == 0) displayMode = std::atoi(val);
            else if (std::strcmp(line, "msaa") == 0) msaaSamples = std::atoi(val);
            else if (std::strcmp(line, "vsync") == 0) vsync = (std::atoi(val) != 0);
            else if (std::strcmp(line, "shadows") == 0) shadows = (std::atoi(val) != 0);
            else if (std::strcmp(line, "sound") == 0) soundEnabled = (std::atoi(val) != 0);
            else if (std::strcmp(line, "serverIp") == 0 && val[0]) std::snprintf(serverIp, sizeof(serverIp), "%s", val);
            else if (std::strcmp(line, "serverPort") == 0) serverPort = std::atoi(val);
            else if (std::strcmp(line, "callsign") == 0 && val[0]) std::snprintf(callsign, sizeof(callsign), "%s", val);
        }
        std::fclose(f);
    }

    void save(const char* path = "config\\settings.ini") const {
        ::CreateDirectoryA("config", nullptr);
        FILE* f = std::fopen(path, "wb");
        if (!f) return;
        std::fprintf(f, "quality=%d\r\n", qualityPreset);
        std::fprintf(f, "displayMode=%d\r\n", displayMode);
        std::fprintf(f, "msaa=%d\r\n", msaaSamples);
        std::fprintf(f, "vsync=%d\r\n", vsync ? 1 : 0);
        std::fprintf(f, "shadows=%d\r\n", shadows ? 1 : 0);
        std::fprintf(f, "sound=%d\r\n", soundEnabled ? 1 : 0);
        std::fprintf(f, "serverIp=%s\r\n", serverIp);
        std::fprintf(f, "serverPort=%d\r\n", serverPort);
        std::fprintf(f, "callsign=%s\r\n", callsign);
        std::fclose(f);
    }
};

struct PlayerProfile {
    int silver = 650000;
    int rp     = 35000;
    int gold   = 2500;
    uint32_t unlockedMask = 0;
    int selectedNation = 1; // По умолчанию открыта Сербия (или Россия) с M-80AB1 / M-84AS1
    int selectedVehPerNation[7] = {0, 5, 8, 11, 14, 17, 20};
    int vehicleCamo[24] = {};

    void initDefaults(const std::vector<VehicleSpec>& vehs) {
        unlockedMask = 0;
        for (size_t i = 0; i < vehs.size() && i < 31; ++i) {
            if (vehs[i].unlockedDefault) unlockedMask |= (1u << i);
        }
        // Дополнительно открываем сербский M-84AS1 сразу для комфортного старта!
        unlockedMask |= (1u << 5);
    }

    bool isUnlocked(int vehIdx) const {
        return (unlockedMask & (1u << vehIdx)) != 0;
    }

    void load(const std::vector<VehicleSpec>& vehs) {
        initDefaults(vehs);
        FILE* f = std::fopen("config\\profile.ini", "rb");
        if (!f) return;
        char line[256];
        while (std::fgets(line, sizeof(line), f)) {
            char* eq = std::strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            char* val = eq + 1;
            if (std::strcmp(line, "silver") == 0) silver = std::atoi(val);
            else if (std::strcmp(line, "rp") == 0) rp = std::atoi(val);
            else if (std::strcmp(line, "gold") == 0) gold = std::atoi(val);
            else if (std::strcmp(line, "unlocked") == 0) unlockedMask |= (uint32_t)std::strtoul(val, nullptr, 10);
            else if (std::strcmp(line, "nation") == 0) selectedNation = std::atoi(val) % 7;
            else if (std::strncmp(line, "camo", 4) == 0) {
                int idx = std::atoi(line + 4);
                if (idx >= 0 && idx < 24) vehicleCamo[idx] = std::atoi(val) % 6;
            }
        }
        std::fclose(f);
    }

    void save() const {
        ::CreateDirectoryA("config", nullptr);
        FILE* f = std::fopen("config\\profile.ini", "wb");
        if (!f) return;
        std::fprintf(f, "silver=%d\r\n", silver);
        std::fprintf(f, "rp=%d\r\n", rp);
        std::fprintf(f, "gold=%d\r\n", gold);
        std::fprintf(f, "unlocked=%u\r\n", unlockedMask);
        std::fprintf(f, "nation=%d\r\n", selectedNation);
        for (int i = 0; i < 23; ++i) std::fprintf(f, "camo%d=%d\r\n", i, vehicleCamo[i]);
        std::fclose(f);
    }
};

} // namespace sf
