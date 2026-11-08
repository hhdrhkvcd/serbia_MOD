#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../src/sim_server.h"
#include "../src/settings_profile.h"
#include <cstdio>

using namespace sf;

static void printLocalIps(uint16_t port) {
    char host[256] = {};
    if (::gethostname(host, sizeof(host)) == 0) {
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        if (::getaddrinfo(host, nullptr, &hints, &res) == 0 && res) {
            std::printf("  Локальные IPv4-адреса для подключения друзей (LAN / Radmin VPN):\n");
            for (addrinfo* p = res; p; p = p->ai_next) {
                auto* sin = (sockaddr_in*)p->ai_addr;
                char ip[64] = {};
                ::inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
                std::printf("    -> %s:%u\n", ip, port);
            }
            ::freeaddrinfo(res);
        }
    }
}

int main(int argc, char** argv) {
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);
    ::SetConsoleTitleW(L"Steel Front: Online — Dagor Engine 6.5 Dedicated Server (UDP)");

    char exePath[MAX_PATH] = {};
    ::GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (char* sl = std::strrchr(exePath, '\\')) *sl = 0;
    ::SetCurrentDirectoryA(exePath);
    if (::GetFileAttributesA("..\\..\\application.blk") != INVALID_FILE_ATTRIBUTES) {
        ::SetCurrentDirectoryA("..\\..");
    }

    initOrReloadDagorGamedata(false);
    const auto& dk = dagor::RuntimeKernel::instance();

    WSADATA wsa{};
    ::WSAStartup(MAKEWORD(2, 2), &wsa);

    GameSettings cfg;
    cfg.load();
    uint16_t port = (uint16_t)cfg.serverPort;
    int startMap = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = (uint16_t)std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--map") == 0 && i + 1 < argc) startMap = std::atoi(argv[++i]);
        else if (i == 1 && argv[i][0] != '-') port = (uint16_t)std::atoi(argv[i]);
    }

    SimServer server;
    if (!server.start(port, startMap)) {
        std::printf("[ОШИБКА] Не удалось занять UDP-порт %u (возможно, сервер уже запущен).\n", port);
        ::Sleep(4000);
        ::WSACleanup();
        return 1;
    }

    std::printf("====================================================================\n");
    std::printf("  STEEL FRONT: ONLINE — DAGOR ENGINE 6.5 DEDICATED SERVER (Win64)\n");
    std::printf("  Ядро Dagor: %d .BLK машин | %d daECS шаблонов | VROMFS файлов: %zu\n",
                dk.loadedVehiclesBlk, dk.entityMgr.templateCount(), dk.vfs.packedFileCount());
    std::printf("  Порт UDP: %u   |   Тикрейт симуляции: 60 Гц (снапшоты 25 Гц)\n", port);
    std::printf("  Текущая карта: %s (%s)\n", getMaps()[server.mapIdx].name, getMaps()[server.mapIdx].subtitle);
    printLocalIps(port);
    std::printf("  Укажите этот IP в Лаунчере SteelFrontLauncher.exe у игроков.\n");
    std::printf("  Для остановки сервера нажмите Ctrl+C или закройте окно.\n");
    std::printf("====================================================================\n\n");

    LARGE_INTEGER freq{}, prev{};
    ::QueryPerformanceFrequency(&freq);
    ::QueryPerformanceCounter(&prev);
    float logTimer = 0.f;

    while (true) {
        LARGE_INTEGER now{};
        ::QueryPerformanceCounter(&now);
        float dt = float(double(now.QuadPart - prev.QuadPart) / double(freq.QuadPart));
        prev = now;
        dt = clampf(dt, 0.001f, 0.05f);

        server.tick(dt);

        logTimer += dt;
        if (logTimer >= 6.0f) {
            logTimer = 0.f;
            int humans = 0, bots = 0;
            for (int i = 0; i < MAX_NET_TANKS; ++i) {
                if (!server.tanks[i].active) continue;
                if (server.tanks[i].hasClient) humans++;
                else if (server.tanks[i].isBot) bots++;
            }
            std::printf("[СЕРВЕР] Карта: %-24s | Живых игроков: %d | Ботов ИИ: %d | Очки: %4.0f : %4.0f\n",
                        getMaps()[server.mapIdx].name, humans, bots, server.tickets[0], server.tickets[1]);
        }
        ::Sleep(14);
    }

    server.stop();
    ::WSACleanup();
    return 0;
}
