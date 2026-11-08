#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "math3d.h"
#include "game_data.h"
#include "net_protocol.h"
#include "sim_server.h"
#include "steam_bridge.h"
#include "sound_winmm.h"
#include "d3d11_renderer.h"
#include "tank_sculptor.h"
#include "ui_d3d11.h"
#include "settings_profile.h"

#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>

using namespace sf;

// Устанавливаем рабочую папку в корень игры (работает и из корня, и из bin\win64\, и из Steam)
static void ensureGameRootWorkingDir() {
    char exePath[MAX_PATH] = {};
    ::GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char* slash = std::strrchr(exePath, '\\');
    if (slash) *slash = 0;
    ::SetCurrentDirectoryA(exePath);

    // Если запущены из подпапки bin\win64 или game\, поднимаемся к корню с application.blk / SteelFrontLauncher.exe
    if (::GetFileAttributesA("..\\..\\application.blk") != INVALID_FILE_ATTRIBUTES ||
        ::GetFileAttributesA("..\\..\\SteelFrontLauncher.exe") != INVALID_FILE_ATTRIBUTES) {
        ::SetCurrentDirectoryA("..\\..");
    } else if (::GetFileAttributesA("..\\application.blk") != INVALID_FILE_ATTRIBUTES) {
        ::SetCurrentDirectoryA("..");
    }
}

struct VisualTracer {
    Vec3  pos;
    Vec3  vel;
    Vec4  color;
    float ttl = 1.5f;
    float scale = 1.f;
    bool  gravity = true;
};

struct VisualPuff {
    Vec3  pos;
    Vec3  vel;
    Vec4  color;
    float radius = 0.5f;
    float grow = 2.5f;
    float age = 0.f;
    float life = 1.2f;
};

struct FlyingTurret {
    int   vehIdx = 0;
    Vec3  pos;
    Vec3  vel;
    Vec3  rot;
    Vec3  angVel;
    float life = 4.2f;
};

struct KillFeedItem {
    std::string killer;
    std::string victim;
    std::string weapon;
    int killerTeam = 0;
    int victimTeam = 1;
    bool ammoDet = false;
    float timer = 8.0f;
};

struct ChatLine {
    int team = 2;
    std::string sender;
    std::string text;
    float timer = 16.f;
};

struct UiHitBox {
    int id = 0;
    int param = 0;
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float mx, float my) const {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    }
};

enum UiActionId {
    ACT_SELECT_NATION = 1,
    ACT_SELECT_VEHICLE,
    ACT_SELECT_SHELL,
    ACT_SELECT_CAMO,
    ACT_SELECT_MAP,
    ACT_TOGGLE_HANGAR_SCENE,
    ACT_SET_INSPECTOR,
    ACT_TOGGLE_TECH_TREE,
    ACT_UNLOCK_VEHICLE,
    ACT_ENTER_BATTLE,
    ACT_ENTER_POLIGON,
    ACT_TOGGLE_SOUND,
    ACT_EXIT_GAME,
    ACT_PAUSE_RESUME,
    ACT_PAUSE_RETURN_HANGAR,
    ACT_OPEN_BLK_CDK,
    ACT_HOT_RELOAD_BLK
};

class SteelFrontApp {
public:
    using Color4 = sf::Color4;
    HWND hwnd = nullptr;
    bool running = true;
    bool isBorderless = true;
    RECT windowedRect{100, 80, 1700, 980};

    GameSettings  settings;
    PlayerProfile profile;
    SteamBridge   steam;
    SoundSystem   sound;
    D3D11Renderer gfx;
    UiRendererD3D11 ui;

    std::vector<VehicleGpuModel> vehModels;
    EnvironmentGpuAssets envAssets;

    // Встроенный авторитативный хост-сервер + клиентский UDP-сокет
    SimServer localServer;
    SOCKET    clientSock = INVALID_SOCKET;
    sockaddr_in serverAddr{};
    int       mySlot = -1;
    int       myTeam = 0;
    float     netPingMs = 1.0f;
    uint8_t   onlineHumans = 1;
    float     pingTimer = 0.f;
    float     joinRetryTimer = 0.f;
    float     stateSendTimer = 0.f;

    // Состояние экрана: 0 = Ангар, 1 = Онлайн-бой / Полигон
    int   appState = 0;
    bool  hangarBunkerMode = false;
    int   inspectorMode = 0; // 0=Обычный, 1=Броня, 2=Рентген X-Ray
    bool  showTechTree = false;
    bool  pauseMenuOpen = false;
    int   selectedMapChoice = 0; // 0..2 Боевые карты, 3 Полигон
    int   selectedShellIdx = 0;

    // Камера ангара
    float hangarYaw = 0.62f;
    float hangarPitch = 0.24f;
    float hangarDist = 8.4f;
    bool  hangarDragging = false;
    POINT lastMousePt{0, 0};
    float mouseX = 0.f, mouseY = 0.f;
    std::vector<UiHitBox> hitBoxes;

    // Параметры боя и локального управления танком
    PktWorldSnapshot worldSnap{};
    bool  hasWorldSnap = false;
    Vec3  renderTankPos[MAX_NET_TANKS];
    float renderHullYaw[MAX_NET_TANKS] = {};
    float renderTurYaw[MAX_NET_TANKS]  = {};
    float renderGunPitch[MAX_NET_TANKS]= {};
    bool  tankInterpInit[MAX_NET_TANKS]= {};

    float myX = 0.f, myY = 0.f, myZ = 0.f;
    float myHullYaw = 0.f, myTurretYaw = 0.f, myGunPitch = 0.f, mySpeed = 0.f;
    float gunRecoil = 0.f;
    float prevReloadTimer = 0.f;
    float mgCooldown = 0.f;

    // Прицеливание в стиле War Thunder (два круга: камера + реальный ствол)
    // camPitch: угол возвышения линии визирования (+вверх, -вниз)
    float camYaw = 0.f;
    float camPitch = -0.02f;
    bool  sniperMode = false;
    float sniperZoom = 5.0f; // 4x..12x
    bool  thermalMode = false;
    int   measuredRangeM = 0;
    Vec3  commandAimWorld{0, 0, -100};
    Vec3  actualBoreWorld{0, 0, -100};
    bool  mouseCaptured = false;

    // 3D Камера попаданий (правый верхний угол)
    PktHitCam activeHitCam{};
    float     hitCamTimer = 0.f;

    // Уведомления о наградах, киллфид и чат
    std::string bannerTitle;
    std::string bannerSub;
    float       bannerTimer = 0.f;
    std::vector<KillFeedItem> killFeed;
    std::vector<ChatLine> chatLines;
    bool        chatInputActive = false;
    std::string chatDraft;

    // Спецэффекты боя
    std::vector<VisualTracer> tracers;
    std::vector<VisualPuff>   puffs;
    std::vector<FlyingTurret> flyingTurrets;

    double appTimeSec = 0.0;
    float  fpsSmoothed = 60.f;

    int currentVehIdx() const {
        return profile.selectedVehPerNation[profile.selectedNation];
    }

    bool init(HWND window, int initW, int initH) {
        hwnd = window;
        const auto& vehs = getVehicles();
        settings.load();
        profile.load(vehs);

        WSADATA wsa{};
        ::WSAStartup(MAKEWORD(2, 2), &wsa);

        steam.init();
        if (!steam.personaName.empty() && std::strcmp(settings.callsign, "Командир") == 0) {
            std::snprintf(settings.callsign, sizeof(settings.callsign), "%s", steam.personaName.c_str());
        }

        sound.enabled = settings.soundEnabled;
        sound.init();

        int msaa = (settings.qualityPreset >= 2) ? settings.msaaSamples : 1;
        bool shd = (settings.qualityPreset >= 1) && settings.shadows;
        if (!gfx.init(hwnd, initW, initH, msaa, settings.vsync, shd)) {
            ::MessageBoxW(hwnd, L"Не удалось инициализировать DirectX 11!", L"Steel Front: Online", MB_ICONERROR);
            return false;
        }
        if (!ui.init(gfx.device, gfx.ctx)) {
            ::MessageBoxW(hwnd, L"Не удалось создать шрифтовой атлас интерфейса!", L"Steel Front: Online", MB_ICONERROR);
            return false;
        }

        vehModels.resize(vehs.size());
        for (size_t i = 0; i < vehs.size(); ++i) {
            buildVehicleMeshes(gfx.device, vehs[i], vehModels[i]);
        }
        buildEnvironmentAssets(gfx.device, envAssets);

        initClientSocket();
        ensureLocalServerIfNeeded(0);
        return true;
    }

    void initClientSocket() {
        clientSock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (clientSock != INVALID_SOCKET) {
            u_long nb = 1;
            ::ioctlsocket(clientSock, FIONBIO, &nb);
        }
        updateServerEndpoint();
    }

    void updateServerEndpoint() {
        serverAddr = sockaddr_in{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons((uint16_t)settings.serverPort);
        serverAddr.sin_addr.s_addr = ::inet_addr(settings.serverIp);
        if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
            serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        }
    }

    bool isLoopbackTarget() const {
        return std::strcmp(settings.serverIp, "127.0.0.1") == 0 ||
               std::strcmp(settings.serverIp, "localhost") == 0 ||
               settings.serverIp[0] == 0;
    }

    void ensureLocalServerIfNeeded(int preferredMap) {
        if (isLoopbackTarget() && !localServer.isRunning()) {
            // Если внешний SteelFrontServer.exe уже запущен на этом порту, bind просто вернёт false и мы подключимся к нему!
            localServer.start((uint16_t)settings.serverPort, preferredMap);
        }
    }

    void sendPacket(const void* data, int len) {
        if (clientSock != INVALID_SOCKET) {
            ::sendto(clientSock, (const char*)data, len, 0, (const sockaddr*)&serverAddr, sizeof(serverAddr));
        }
    }

    void captureMouse(bool cap) {
        mouseCaptured = cap;
        if (cap) {
            while (::ShowCursor(FALSE) >= 0) {}
            RECT rc{};
            ::GetClientRect(hwnd, &rc);
            POINT c{(rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2};
            ::ClientToScreen(hwnd, &c);
            ::SetCursorPos(c.x, c.y);
        } else {
            while (::ShowCursor(TRUE) < 0) {}
        }
    }

    void startBattle(int mapIdx) {
        int vIdx = currentVehIdx();
        if (!profile.isUnlocked(vIdx)) {
            // Если выбран неисследованный танк для осмотра, берём первый открытый у этой нации
            const auto& vehs = getVehicles();
            for (size_t i = 0; i < vehs.size(); ++i) {
                if (vehs[i].nationIdx == profile.selectedNation && profile.isUnlocked((int)i)) {
                    vIdx = (int)i;
                    profile.selectedVehPerNation[profile.selectedNation] = vIdx;
                    break;
                }
            }
        }

        ensureLocalServerIfNeeded(mapIdx);
        if (localServer.isRunning() && localServer.countHumans() <= 1 && localServer.mapIdx != mapIdx) {
            localServer.setupMap(mapIdx);
        }

        mySlot = -1;
        hasWorldSnap = false;
        for (bool& b : tankInterpInit) b = false;
        tracers.clear();
        puffs.clear();
        flyingTurrets.clear();
        killFeed.clear();
        sniperMode = false;
        thermalMode = false;
        pauseMenuOpen = false;
        chatInputActive = false;
        hitCamTimer = 0.f;

        PktJoin j;
        j.vehicleIdx = (uint8_t)vIdx;
        j.preferredMapIdx = (uint8_t)mapIdx;
        std::snprintf(j.callsign, sizeof(j.callsign), "%s", settings.callsign);
        sendPacket(&j, sizeof(j));
        joinRetryTimer = 0.45f;

        appState = 1;
        captureMouse(true);
        sound.play(SND_UI_CLICK);

        char rich[96];
        std::snprintf(rich, sizeof(rich), "В бою на %s • %s",
                      getVehicles()[vIdx].shortName, getMaps()[mapIdx].name);
        steam.setStatus(rich);
    }

    void returnToHangar() {
        if (mySlot >= 0) {
            PktAction act;
            act.slot = (uint8_t)mySlot;
            act.action = 5; // leave
            sendPacket(&act, sizeof(act));
        }
        mySlot = -1;
        appState = 0;
        pauseMenuOpen = false;
        chatInputActive = false;
        sniperMode = false;
        thermalMode = false;
        captureMouse(false);
        profile.save();
        steam.setStatus("В ангаре • Steel Front: Online");
    }

    void spawnExplosionFx(const Vec3& p, bool bigAmmoBlowoff, int targetVehIdx = 0) {
        int count = bigAmmoBlowoff ? 22 : 12;
        for (int i = 0; i < count; ++i) {
            VisualPuff pf;
            pf.pos = p + Vec3(((std::rand() % 100) - 50) * 0.03f, 0.4f + (std::rand() % 100) * 0.015f, ((std::rand() % 100) - 50) * 0.03f);
            pf.vel = Vec3(((std::rand() % 100) - 50) * 0.14f, 2.5f + (std::rand() % 100) * 0.08f, ((std::rand() % 100) - 50) * 0.14f);
            bool fire = (i < count / 2);
            pf.color = fire ? Vec4(1.0f, 0.52f, 0.12f, 0.92f) : Vec4(0.16f, 0.15f, 0.14f, 0.78f);
            pf.radius = fire ? 0.85f : 1.15f;
            pf.grow   = fire ? 4.2f : 3.4f;
            pf.life   = fire ? 0.65f : 2.1f;
            puffs.push_back(pf);
        }
        if (bigAmmoBlowoff) {
            FlyingTurret ft;
            ft.vehIdx = targetVehIdx;
            ft.pos = p + Vec3(0.f, 1.35f, 0.f);
            ft.vel = Vec3(((std::rand() % 100) - 50) * 0.06f, 10.5f, ((std::rand() % 100) - 50) * 0.06f);
            ft.rot = Vec3(0, 0, 0);
            ft.angVel = Vec3(3.4f, 2.2f, 2.8f);
            ft.life = 4.5f;
            flyingTurrets.push_back(ft);
        }
    }

    void pollClientNetwork() {
        if (clientSock == INVALID_SOCKET) return;
        uint8_t buf[2048];
        sockaddr_in from{};
        int fromLen = sizeof(from);
        while (true) {
            fromLen = sizeof(from);
            int r = ::recvfrom(clientSock, (char*)buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
            if (r <= (int)sizeof(PktHeader)) break;
            const auto* hdr = reinterpret_cast<const PktHeader*>(buf);
            if (hdr->magic != NET_MAGIC) continue;

            if (hdr->type == PKT_PONG && r >= (int)sizeof(PktPong)) {
                const auto* p = reinterpret_cast<const PktPong*>(buf);
                uint32_t nowMs = ::GetTickCount();
                netPingMs = std::max(0.5f, float(nowMs - p->clientTimeMs));
                onlineHumans = p->onlineHumans;
            }
            else if (hdr->type == PKT_JOIN_ACK && r >= (int)sizeof(PktJoinAck)) {
                const auto* ack = reinterpret_cast<const PktJoinAck*>(buf);
                mySlot = ack->assignedSlot;
                myTeam = ack->team;
                const auto& spList = (myTeam == 0 || getMaps()[ack->mapIdx].redSpawns.empty())
                    ? getMaps()[ack->mapIdx].blueSpawns : getMaps()[ack->mapIdx].redSpawns;
                const SpawnPoint& sp = spList[mySlot % spList.size()];
                myX = sp.x; myZ = sp.z;
                myY = getTerrainHeight(ack->mapIdx, myX, myZ);
                myHullYaw = myTurretYaw = camYaw = sp.yaw;
                camPitch = -0.02f;
                myGunPitch = 0.f;
                mySpeed = 0.f;
            }
            else if (hdr->type == PKT_WORLD_SNAPSHOT && r >= (int)sizeof(PktWorldSnapshot)) {
                const auto* ws = reinterpret_cast<const PktWorldSnapshot*>(buf);
                bool wasDead = (hasWorldSnap && mySlot >= 0 && !worldSnap.tanks[mySlot].alive);
                worldSnap = *ws;
                hasWorldSnap = true;

                if (mySlot >= 0 && mySlot < MAX_NET_TANKS && worldSnap.tanks[mySlot].active) {
                    const NetTankSnap& me = worldSnap.tanks[mySlot];
                    // При возрождении синхронизируем позицию от сервера
                    if ((wasDead && me.alive) || std::hypot(me.x - myX, me.z - myZ) > 18.f) {
                        myX = me.x; myY = me.y; myZ = me.z;
                        myHullYaw = myTurretYaw = me.hullYaw;
                    }
                    if (prevReloadTimer > 0.01f && me.reloadTimer <= 0.001f) {
                        sound.play(SND_RELOAD_READY, 0.75f);
                    }
                    prevReloadTimer = me.reloadTimer;
                }
            }
            else if (hdr->type == PKT_FX_EVENT && r >= (int)sizeof(PktFxEvent)) {
                const auto* fx = reinterpret_cast<const PktFxEvent*>(buf);
                Vec3 pos(fx->x, fx->y, fx->z);
                Vec3 dir(fx->dx, fx->dy, fx->dz);
                float distToCam = (pos - Vec3(myX, myY, myZ)).length();
                float atten = clampf(1.f - distToCam / 320.f, 0.12f, 1.f);

                if (fx->fxKind == FX_CANNON_FIRE) {
                    bool isAuto = (fx->shooterOrTargetSlot < MAX_NET_TANKS &&
                                   getVehicles()[worldSnap.tanks[fx->shooterOrTargetSlot].vehicleIdx].arch == ARCH_IFV &&
                                   fx->speed > 10.f);
                    sound.play(isAuto ? SND_AUTOCANNON : SND_CANNON, atten);
                    if (fx->shooterOrTargetSlot == mySlot) gunRecoil = 0.42f;

                    // Дульная вспышка и пылевое облако выстрела
                    puffs.push_back({pos, dir * 4.f, Vec4(1.0f, 0.72f, 0.22f, 0.95f), 0.55f, 5.5f, 0.f, 0.14f});
                    puffs.push_back({pos + dir * 1.2f, dir * 3.f + Vec3(0,1.2f,0), Vec4(0.58f, 0.55f, 0.50f, 0.45f), 0.7f, 3.2f, 0.f, 0.85f});
                    if (fx->speed > 1.f) {
                        VisualTracer tr;
                        tr.pos = pos;
                        tr.vel = dir * fx->speed;
                        tr.color = (fx->shellType == SHELL_APFSDS) ? Vec4(1.0f, 0.38f, 0.18f, 1.f) : Vec4(1.0f, 0.84f, 0.28f, 1.f);
                        tr.ttl = 2.6f;
                        tr.scale = 1.0f;
                        tr.gravity = true;
                        tracers.push_back(tr);
                    }
                }
                else if (fx->fxKind == FX_MG_TRACER) {
                    sound.play(SND_MG, atten * 0.65f);
                    VisualTracer tr;
                    tr.pos = pos;
                    tr.vel = dir * fx->speed;
                    tr.color = Vec4(1.0f, 0.76f, 0.22f, 0.95f);
                    tr.ttl = 1.1f;
                    tr.scale = 0.42f;
                    tr.gravity = false;
                    tracers.push_back(tr);
                }
                else if (fx->fxKind == FX_SPARK_RICOCHET) {
                    sound.play(SND_RICOCHET, atten);
                    for (int i = 0; i < 6; ++i) {
                        puffs.push_back({pos, Vec3(((std::rand()%100)-50)*0.12f, 3.f+(std::rand()%50)*0.08f, ((std::rand()%100)-50)*0.12f),
                                         Vec4(1.0f, 0.82f, 0.25f, 0.95f), 0.18f, 1.2f, 0.f, 0.32f});
                    }
                }
                else if (fx->fxKind == FX_ARMOR_PEN) {
                    sound.play(SND_PENETRATION, atten);
                    for (int i = 0; i < 8; ++i) {
                        puffs.push_back({pos, Vec3(((std::rand()%100)-50)*0.10f, 2.2f+(std::rand()%50)*0.06f, ((std::rand()%100)-50)*0.10f),
                                         Vec4(1.0f, 0.45f, 0.12f, 0.92f), 0.32f, 2.2f, 0.f, 0.48f});
                    }
                }
                else if (fx->fxKind == FX_DIRT_HIT) {
                    for (int i = 0; i < 5; ++i) {
                        puffs.push_back({pos, Vec3(((std::rand()%100)-50)*0.07f, 2.8f+(std::rand()%40)*0.06f, ((std::rand()%100)-50)*0.07f),
                                         Vec4(0.36f, 0.31f, 0.24f, 0.70f), 0.45f, 2.6f, 0.f, 0.85f});
                    }
                }
                else if (fx->fxKind == FX_TANK_EXPLOSION || fx->fxKind == FX_AMMO_BLOWOFF) {
                    sound.play(SND_EXPLOSION, std::max(0.35f, atten));
                    int vIdx = (fx->shooterOrTargetSlot < MAX_NET_TANKS) ? worldSnap.tanks[fx->shooterOrTargetSlot].vehicleIdx : 0;
                    spawnExplosionFx(pos, fx->fxKind == FX_AMMO_BLOWOFF, vIdx);
                }
                else if (fx->fxKind == FX_SMOKE_SCREEN) {
                    for (int i = 0; i < 10; ++i) {
                        Vec3 off(((std::rand()%100)-50)*0.12f, 0.6f + (i % 3)*0.8f, ((std::rand()%100)-50)*0.12f);
                        puffs.push_back({pos + off, Vec3(0.f, 0.35f, 0.f), Vec4(0.84f, 0.86f, 0.88f, 0.68f), 2.4f, 0.55f, 0.f, 14.5f});
                    }
                }
            }
            else if (hdr->type == PKT_HIT_CAM && r >= (int)sizeof(PktHitCam)) {
                activeHitCam = *reinterpret_cast<const PktHitCam*>(buf);
                hitCamTimer = 5.4f;
                profile.silver += activeHitCam.rewardSilver;
                profile.rp     += activeHitCam.rewardRp;

                if (activeHitCam.result == HIT_KILL) {
                    bannerTitle = activeHitCam.ammoDetonation ? "ТАНК УНИЧТОЖЕН • ДЕТОНАЦИЯ БК!" : "ТАНК ПРОТИВНИКА УНИЧТОЖЕН";
                    char sub[128];
                    std::snprintf(sub, sizeof(sub), "+%u СЛ   +%u ОИ   (%s)", activeHitCam.rewardSilver, activeHitCam.rewardRp, activeHitCam.targetName);
                    bannerSub = sub;
                    bannerTimer = 3.8f;
                } else if (activeHitCam.result == HIT_CRIT) {
                    bannerTitle = "КРИТИЧЕСКОЕ ПОВРЕЖДЕНИЕ";
                    char sub[128];
                    std::snprintf(sub, sizeof(sub), "%s • +%u СЛ +%u ОИ", activeHitCam.zoneName, activeHitCam.rewardSilver, activeHitCam.rewardRp);
                    bannerSub = sub;
                    bannerTimer = 2.4f;
                }
            }
            else if (hdr->type == PKT_KILL_FEED && r >= (int)sizeof(PktKillFeed)) {
                const auto* kf = reinterpret_cast<const PktKillFeed*>(buf);
                killFeed.insert(killFeed.begin(), {kf->killerName, kf->victimName, kf->weaponCode, kf->killerTeam, kf->victimTeam, kf->ammoDetonation != 0, 8.5f});
                if (killFeed.size() > 6) killFeed.pop_back();
            }
            else if (hdr->type == PKT_CHAT && r >= (int)sizeof(PktChat)) {
                const auto* ch = reinterpret_cast<const PktChat*>(buf);
                chatLines.push_back({ch->team, ch->sender, ch->text, 18.f});
                if (chatLines.size() > 7) chatLines.erase(chatLines.begin());
            }
        }
    }

    void updateBattlePlayer(float dt) {
        if (!hasWorldSnap || mySlot < 0 || mySlot >= MAX_NET_TANKS) return;
        const NetTankSnap& me = worldSnap.tanks[mySlot];
        const MapSpec& map = getMaps()[worldSnap.mapIdx];
        const VehicleSpec& veh = getVehicles()[me.vehicleIdx];

        if (gunRecoil > 0.f) gunRecoil = std::max(0.f, gunRecoil - dt * 1.6f);
        if (mgCooldown > 0.f) mgCooldown = std::max(0.f, mgCooldown - dt);

        if (me.alive && !pauseMenuOpen && !chatInputActive) {
            bool canDrive = (me.flags & 2) == 0 && me.modHealth[0] > 0 && me.modHealth[2] > 0;
            float throttle = 0.f, turn = 0.f;
            if ((::GetAsyncKeyState('W') & 0x8000) || (::GetAsyncKeyState(VK_UP) & 0x8000))    throttle += 1.f;
            if ((::GetAsyncKeyState('S') & 0x8000) || (::GetAsyncKeyState(VK_DOWN) & 0x8000))  throttle -= 1.f;
            // D / Вправо -> поворот вправо (+yaw), A / Влево -> поворот влево (-yaw)
            if ((::GetAsyncKeyState('D') & 0x8000) || (::GetAsyncKeyState(VK_RIGHT) & 0x8000)) turn += 1.f;
            if ((::GetAsyncKeyState('A') & 0x8000) || (::GetAsyncKeyState(VK_LEFT) & 0x8000))  turn -= 1.f;

            if (canDrive) {
                myHullYaw = wrapAngle(myHullYaw + turn * deg2rad(veh.hullDegSec) * dt);
                float maxFwd = veh.fwdKmh / 3.6f;
                float maxRev = veh.revKmh / 3.6f;
                float targetSpd = (throttle > 0.f) ? maxFwd : ((throttle < 0.f) ? -maxRev : 0.f);
                mySpeed = lerpf(mySpeed, targetSpd, dt * (throttle != 0.f ? 2.8f : 4.2f));
                float nx = myX - std::sin(myHullYaw) * mySpeed * dt;
                float nz = myZ - std::cos(myHullYaw) * mySpeed * dt;
                resolveMapCollision(map, nx, nz, 2.45f);
                myX = nx; myZ = nz;
                myY = getTerrainHeight(worldSnap.mapIdx, myX, myZ);
            } else {
                mySpeed *= 0.85f;
            }

            // Единый вектор прицеливания камеры для вида от 3-го лица и снайперского прицела
            Vec3 camForward(
                -std::sin(camYaw) * std::cos(camPitch),
                 std::sin(camPitch),
                -std::cos(camYaw) * std::cos(camPitch)
            );
            Vec3 camOrigin;
            if (sniperMode) {
                camOrigin = Vec3(myX - std::sin(myTurretYaw) * 1.15f, myY + 1.94f, myZ - std::cos(myTurretYaw) * 1.15f);
            } else {
                Vec3 pivot(myX, myY + 2.55f, myZ);
                camOrigin = pivot - camForward * 9.4f + Vec3(0.f, 0.85f, 0.f);
                float gndH = getTerrainHeight(worldSnap.mapIdx, camOrigin.x, camOrigin.z) + 0.65f;
                if (camOrigin.y < gndH) camOrigin.y = gndH;
            }

            if (!(::GetAsyncKeyState('C') & 0x8000)) {
                // Трассируем командную точку прицеливания по ландшафту, объектам и вражеским танкам
                Vec3 rayEnd = camOrigin + camForward * 650.f;
                Vec3 hitPt = rayEnd;
                bool dummyG = false;
                checkSegmentBlocked(map, camOrigin + camForward * 4.f, rayEnd, &hitPt, &dummyG);
                for (int i = 0; i < MAX_NET_TANKS; ++i) {
                    if (i == mySlot || !worldSnap.tanks[i].active || !worldSnap.tanks[i].alive) continue;
                    Vec3 tc = renderTankPos[i] + Vec3(0, 1.25f, 0);
                    Vec3 seg = hitPt - camOrigin;
                    float u = clampf(Vec3::dot(tc - camOrigin, seg) / (seg.lengthSq() + 1e-5f), 0.f, 1.f);
                    Vec3 cp = camOrigin + seg * u;
                    if ((cp - tc).length() < 2.3f && u > 0.02f) hitPt = cp;
                }
                commandAimWorld = hitPt;
            }

            // Плавно вращаем башню и возвышаем ствол к командной точке прицеливания
            Vec3 trunnion(myX, myY + 1.72f, myZ);
            Vec3 toAim = commandAimWorld - trunnion;
            float desTurYaw = std::atan2(-toAim.x, -toAim.z);
            float ringFactor = std::max(0.18f, me.modHealth[3] / 100.f);
            myTurretYaw = moveAngleToward(myTurretYaw, desTurYaw, deg2rad(veh.turretDegSec) * ringFactor * dt);

            float horiz = std::max(1.f, std::hypot(toAim.x, toAim.z));
            float desPitch = clampf(std::atan2(toAim.y, horiz), -0.15f, 0.38f);
            myGunPitch = lerpf(myGunPitch, desPitch, std::min(1.f, dt * 8.5f));

            // Реальная точка, куда смотрит ствол орудия (второй круг War Thunder!)
            Vec3 boreDir(
                -std::sin(myTurretYaw) * std::cos(myGunPitch),
                 std::sin(myGunPitch),
                -std::cos(myTurretYaw) * std::cos(myGunPitch)
            );
            Vec3 boreEnd = trunnion + boreDir * 550.f;
            Vec3 boreHit = boreEnd;
            bool dg = false;
            checkSegmentBlocked(map, trunnion + boreDir * 4.2f, boreEnd, &boreHit, &dg);
            actualBoreWorld = boreHit;

            // Выстрел основным орудием (ЛКМ)
            if ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) && me.reloadTimer <= 0.001f && GetForegroundWindow() == hwnd) {
                Vec3 muzzle = trunnion + boreDir * 3.9f;
                PktFire pf;
                pf.slot = (uint8_t)mySlot;
                pf.shellIdx = (uint8_t)selectedShellIdx;
                pf.ox = muzzle.x; pf.oy = muzzle.y; pf.oz = muzzle.z;
                pf.dx = boreDir.x; pf.dy = boreDir.y; pf.dz = boreDir.z;
                sendPacket(&pf, sizeof(pf));
            }

            // Очередь из спаренного/зенитного пулемёта (Пробел)
            if ((::GetAsyncKeyState(VK_SPACE) & 0x8000) && mgCooldown <= 0.f && GetForegroundWindow() == hwnd) {
                mgCooldown = 0.095f;
                Vec3 muzzle = trunnion + Vec3(0.18f, 0.15f, 0.f) + boreDir * 2.4f;
                PktAction act;
                act.slot = (uint8_t)mySlot;
                act.action = 4;
                act.ox = muzzle.x; act.oy = muzzle.y; act.oz = muzzle.z;
                act.dx = boreDir.x; act.dy = boreDir.y; act.dz = boreDir.z;
                sendPacket(&act, sizeof(act));
            }

            // Лазерное наведение ПТУР в полёте
            PktGuideAtgm ga;
            ga.slot = (uint8_t)mySlot;
            ga.tx = commandAimWorld.x; ga.ty = commandAimWorld.y; ga.tz = commandAimWorld.z;
            sendPacket(&ga, sizeof(ga));
        }

        // Отправка позиции нашего танка на сервер (30 Гц)
        stateSendTimer += dt;
        if (stateSendTimer >= 0.033f) {
            stateSendTimer = 0.f;
            PktClientState cs;
            cs.slot = (uint8_t)mySlot;
            cs.x = myX; cs.z = myZ;
            cs.hullYaw = myHullYaw;
            cs.turretYaw = myTurretYaw;
            cs.gunPitch = myGunPitch;
            cs.speed = mySpeed;
            sendPacket(&cs, sizeof(cs));
        }
    }

    void update(float dt) {
        appTimeSec += dt;
        fpsSmoothed = lerpf(fpsSmoothed, 1.f / std::max(0.0005f, dt), 0.08f);
        steam.update();

        // Шаг встроенного авторитативного сервера (если запущен локально)
        if (localServer.isRunning()) {
            localServer.tick(dt);
        }

        // Пинг сервера раз в 1.5 с
        pingTimer -= dt;
        if (pingTimer <= 0.f) {
            pingTimer = 1.5f;
            PktPing pp;
            pp.clientTimeMs = ::GetTickCount();
            sendPacket(&pp, sizeof(pp));
        }

        pollClientNetwork();

        if (appState == 1) {
            // Повтор пакета входа, если UDP-пакет не дошёл с первой попытки
            if (mySlot < 0) {
                joinRetryTimer -= dt;
                if (joinRetryTimer <= 0.f) {
                    joinRetryTimer = 0.6f;
                    PktJoin j;
                    j.vehicleIdx = (uint8_t)currentVehIdx();
                    j.preferredMapIdx = (uint8_t)selectedMapChoice;
                    std::snprintf(j.callsign, sizeof(j.callsign), "%s", settings.callsign);
                    sendPacket(&j, sizeof(j));
                }
            }

            // Захват мыши в бою (при активном окне и закрытых меню)
            bool wantCap = (!pauseMenuOpen && !chatInputActive && ::GetForegroundWindow() == hwnd);
            if (wantCap != mouseCaptured) captureMouse(wantCap);
            if (mouseCaptured) {
                RECT rc{};
                ::GetClientRect(hwnd, &rc);
                POINT center{(rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2};
                POINT cur{};
                ::GetCursorPos(&cur);
                ::ScreenToClient(hwnd, &cur);
                int dx = cur.x - center.x;
                int dy = cur.y - center.y;
                if (dx != 0 || dy != 0) {
                    float sens = sniperMode ? (0.0018f * (4.f / sniperZoom)) : 0.0025f;
                    // Мышь вправо (dx > 0) -> прицел вправо (+camYaw); мышь вверх (dy < 0) -> прицел вверх (+camPitch)
                    camYaw   = wrapAngle(camYaw + dx * sens);
                    camPitch = clampf(camPitch - dy * sens, -0.42f, 0.48f);
                    POINT scrCenter = center;
                    ::ClientToScreen(hwnd, &scrCenter);
                    ::SetCursorPos(scrCenter.x, scrCenter.y);
                }
            }

            updateBattlePlayer(dt);

            // Сглаженная интерполяция позиций всех сетевых танков + огонь МТО
            if (hasWorldSnap) {
                for (int i = 0; i < MAX_NET_TANKS; ++i) {
                    const NetTankSnap& ts = worldSnap.tanks[i];
                    if (!ts.active) { tankInterpInit[i] = false; continue; }
                    if (i == mySlot && ts.alive) {
                        renderTankPos[i] = Vec3(myX, myY, myZ);
                        renderHullYaw[i] = myHullYaw;
                        renderTurYaw[i]  = myTurretYaw;
                        renderGunPitch[i]= myGunPitch;
                        tankInterpInit[i]= true;
                    } else {
                        Vec3 tgt(ts.x, ts.y, ts.z);
                        if (!tankInterpInit[i] || (tgt - renderTankPos[i]).length() > 15.f) {
                            renderTankPos[i] = tgt;
                            renderHullYaw[i] = ts.hullYaw;
                            renderTurYaw[i]  = ts.turretYaw;
                            renderGunPitch[i]= ts.gunPitch;
                            tankInterpInit[i]= true;
                        } else {
                            float k = std::min(1.f, dt * 14.f);
                            renderTankPos[i] = Vec3::lerp(renderTankPos[i], tgt, k);
                            renderHullYaw[i] = moveAngleToward(renderHullYaw[i], ts.hullYaw, dt * 6.f);
                            renderTurYaw[i]  = moveAngleToward(renderTurYaw[i],  ts.turretYaw, dt * 8.f);
                            renderGunPitch[i]= lerpf(renderGunPitch[i], ts.gunPitch, k);
                        }
                    }

                    // Дым и пламя горящего танка или остова
                    if ((!ts.alive || (ts.flags & 1)) && (std::rand() % 100) < 35) {
                        bool fire = (ts.flags & 1) != 0;
                        puffs.push_back({
                            renderTankPos[i] + Vec3(((std::rand()%40)-20)*0.02f, 1.35f, 1.1f),
                            Vec3(0.2f, fire ? 2.8f : 1.9f, 0.1f),
                            fire ? Vec4(1.0f, 0.45f, 0.10f, 0.82f) : Vec4(0.11f, 0.11f, 0.12f, 0.62f),
                            0.45f, 1.4f, 0.f, fire ? 0.55f : 1.8f
                        });
                    }
                }
            }

            // Обновление визуальных трассеров, клубов дыма и отлетевших башен
            for (int i = (int)tracers.size() - 1; i >= 0; --i) {
                tracers[i].ttl -= dt;
                if (tracers[i].gravity) tracers[i].vel.y -= 9.81f * dt;
                tracers[i].pos = tracers[i].pos + tracers[i].vel * dt;
                if (tracers[i].ttl <= 0.f || (hasWorldSnap && tracers[i].pos.y < getTerrainHeight(worldSnap.mapIdx, tracers[i].pos.x, tracers[i].pos.z)))
                    tracers.erase(tracers.begin() + i);
            }
            for (int i = (int)puffs.size() - 1; i >= 0; --i) {
                puffs[i].age += dt;
                puffs[i].pos = puffs[i].pos + puffs[i].vel * dt;
                puffs[i].vel = puffs[i].vel * std::max(0.f, 1.f - dt * 1.4f);
                puffs[i].radius += puffs[i].grow * dt;
                if (puffs[i].age >= puffs[i].life) puffs.erase(puffs.begin() + i);
            }
            for (int i = (int)flyingTurrets.size() - 1; i >= 0; --i) {
                auto& ft = flyingTurrets[i];
                ft.life -= dt;
                ft.vel.y -= 14.5f * dt;
                ft.pos = ft.pos + ft.vel * dt;
                ft.rot = ft.rot + ft.angVel * dt;
                float gnd = hasWorldSnap ? getTerrainHeight(worldSnap.mapIdx, ft.pos.x, ft.pos.z) : 0.f;
                if (ft.pos.y < gnd + 0.35f) {
                    ft.pos.y = gnd + 0.35f;
                    ft.vel.y = -ft.vel.y * 0.25f;
                    ft.vel.x *= 0.6f; ft.vel.z *= 0.6f;
                    ft.angVel = ft.angVel * 0.5f;
                }
                if (ft.life <= 0.f) flyingTurrets.erase(flyingTurrets.begin() + i);
            }

            if (hitCamTimer > 0.f) hitCamTimer = std::max(0.f, hitCamTimer - dt);
            if (bannerTimer > 0.f) bannerTimer = std::max(0.f, bannerTimer - dt);
            for (int i = (int)killFeed.size() - 1; i >= 0; --i) {
                killFeed[i].timer -= dt;
                if (killFeed[i].timer <= 0.f) killFeed.erase(killFeed.begin() + i);
            }
            for (int i = (int)chatLines.size() - 1; i >= 0; --i) {
                chatLines[i].timer -= dt;
                if (chatLines[i].timer <= 0.f) chatLines.erase(chatLines.begin() + i);
            }
        } else {
            if (mouseCaptured) captureMouse(false);
            if (!hangarDragging) hangarYaw = wrapAngle(hangarYaw + dt * 0.085f);
        }
    }

    // Отрисовка полной сочленённой 3D-модели танка (корпус + башня + качающееся орудие с откатом)
    void drawArticulatedTank(int vehIdx, const Vec3& pos, float hYaw, float tYaw, float gPitch,
                             int camoIdx, float objMode, float burntFactor = 0.f, float recoil = 0.f,
                             bool drawTurretAndGun = true) {
        if (vehIdx < 0 || vehIdx >= (int)vehModels.size()) return;
        const VehicleSpec& vs = getVehicles()[vehIdx];
        const auto& camos = getCamouflages();
        const CamoSpec& cm = camos[camoIdx % camos.size()];
        const VehicleGpuModel& mdl = vehModels[vehIdx];

        Vec3 colA = cm.useVehicleBase ? vs.baseColor : cm.c0;
        CBObject obj{};
        obj.tintAlpha       = Vec4(colA.x, colA.y, colA.z, 1.0f);
        obj.camoColBPattern = Vec4(cm.c1.x, cm.c1.y, cm.c1.z, (float)cm.idx);
        obj.camoColCObjMode = Vec4(cm.c2.x, cm.c2.y, cm.c2.z, objMode);
        obj.armorParams     = Vec4((float)vs.hullArmor[0], (float)vs.hullArmor[1], (float)vs.hullArmor[2], burntFactor);

        // Наклон корпуса по рельефу карты
        Mat4 hullWorld = Mat4::rotationY(hYaw) * Mat4::translation(pos.x, pos.y, pos.z);
        obj.world = hullWorld;
        gfx.drawMesh(mdl.hull, obj);

        if (drawTurretAndGun) {
            obj.armorParams = Vec4((float)vs.turretArmor[0], (float)vs.turretArmor[1], (float)vs.turretArmor[2], burntFactor);
            Mat4 turWorld = Mat4::rotationY(tYaw) * Mat4::translation(pos.x, pos.y, pos.z);
            obj.world = turWorld;
            gfx.drawMesh(mdl.turret, obj);

            // Маска и ствол вращаются вокруг цапф (0, 1.56, -0.45) в пространстве башни + откат вдоль +Z
            const float pivotY = (vs.arch == ARCH_IFV) ? 1.56f : 1.54f;
            Mat4 gunLocal = Mat4::translation(0.f, 0.f, recoil) *
                            Mat4::rotationX(gPitch) *
                            Mat4::translation(0.f, pivotY, -0.45f);
            obj.world = gunLocal * turWorld;
            gfx.drawMesh(mdl.gun, obj);
        }
    }

    void renderScene3D() {
        float aspect = float(gfx.width) / std::max(1, gfx.height);

        if (appState == 0) {
            // =================================================================
            // 3D СЦЕНА АНГАРА (ОТКРЫТАЯ ВОЕННАЯ БАЗА ИЛИ БУНКЕР)
            // =================================================================
            Vec3 target(0.f, 1.28f, 0.f);
            Vec3 camPos(
                target.x + std::sin(hangarYaw) * std::cos(hangarPitch) * hangarDist,
                std::max(0.55f, target.y + std::sin(hangarPitch) * hangarDist),
                target.z + std::cos(hangarYaw) * std::cos(hangarPitch) * hangarDist
            );

            Vec3 sunDir = hangarBunkerMode ? Vec3(0.28f, 0.92f, -0.25f).normalized()
                                           : Vec3(0.62f, 0.50f, -0.60f).normalized();
            Mat4 lightVP = Mat4::lookAtLH(sunDir * 42.f, Vec3(0, 0.8f, 0), Vec3(0, 1, 0)) *
                           Mat4::orthoLH(28.f, 28.f, 4.f, 88.f);
            Mat4 viewProj = Mat4::lookAtLH(camPos, target, Vec3(0, 1, 0)) *
                            Mat4::perspectiveFovLH(deg2rad(45.f), aspect, 0.25f, 760.f);

            CBFrame fb{};
            fb.viewProj      = viewProj;
            fb.lightViewProj = lightVP;
            fb.camPosTime    = Vec4(camPos.x, camPos.y, camPos.z, (float)appTimeSec);
            fb.sunDirQuality = Vec4(sunDir.x, sunDir.y, sunDir.z, gfx.shadowsEnabled ? 1.f : 0.f);
            fb.sunColorExposure = hangarBunkerMode ? Vec4(2.4f, 2.45f, 2.75f, 1.05f) : Vec4(4.45f, 3.85f, 3.05f, 1.12f);
            fb.skyTopFogStart   = hangarBunkerMode ? Vec4(0.08f, 0.10f, 0.14f, 35.f) : Vec4(0.14f, 0.34f, 0.68f, 75.f);
            fb.skyHorizFogEnd   = hangarBunkerMode ? Vec4(0.14f, 0.16f, 0.20f, 98.f) : Vec4(0.64f, 0.75f, 0.88f, 255.f);
            fb.groundAmbMode    = Vec4(0.15f, 0.16f, 0.14f, 0.f);

            int vIdx = currentVehIdx();
            int camoIdx = profile.vehicleCamo[vIdx];
            float idleTurYaw = std::sin((float)appTimeSec * 0.35f) * 0.16f;

            auto drawHangarObjects = [&](bool shadowPass) {
                if (!shadowPass && !hangarBunkerMode) {
                    CBObject skyObj{};
                    skyObj.world = Mat4::translation(camPos.x, 0.f, camPos.z);
                    skyObj.tintAlpha = Vec4(1, 1, 1, 1);
                    gfx.drawMesh(envAssets.skyDome, skyObj);
                }

                CBObject envObj{};
                envObj.world = Mat4::identity();
                envObj.tintAlpha = Vec4(1, 1, 1, 1);
                gfx.drawMesh(hangarBunkerMode ? envAssets.hangarBunker : envAssets.hangarOutdoor, envObj);

                // Фоновые союзные танки на стоянке открытой базы (как в ангаре War Thunder!)
                if (!hangarBunkerMode) {
                    drawArticulatedTank(7,  Vec3(-9.8f, 0.0f, 8.8f),  0.42f,  0.28f, 0.03f, 1, 0.f);
                    drawArticulatedTank(3,  Vec3( 9.8f, 0.0f, 9.2f), -0.45f, -0.32f, 0.02f, 1, 0.f);
                }

                if (inspectorMode == 2 && !shadowPass) {
                    // Режим РЕНТГЕН (X-Ray): внутренние модули + полупрозрачная голографическая броня
                    CBObject xMod{};
                    xMod.world = Mat4::identity();
                    xMod.tintAlpha = Vec4(1, 1, 1, 1);
                    gfx.drawMesh(vehModels[vIdx].xrayModules, xMod);

                    gfx.setAlphaBlend(true);
                    drawArticulatedTank(vIdx, Vec3(0, 0, 0), 0.f, idleTurYaw, 0.04f, camoIdx, 3.0f);
                    gfx.setAlphaBlend(false);
                } else {
                    float modeFlag = (inspectorMode == 1) ? 2.0f : 0.0f;
                    drawArticulatedTank(vIdx, Vec3(0, 0, 0), 0.f, idleTurYaw, 0.04f, camoIdx, modeFlag);
                }
            };

            if (gfx.shadowsEnabled) {
                gfx.beginShadowPass(fb);
                drawHangarObjects(true);
            }
            gfx.beginMainPass(fb, hangarBunkerMode ? Vec4(0.06f, 0.07f, 0.09f, 1.f) : Vec4(0.26f, 0.44f, 0.68f, 1.f));
            drawHangarObjects(false);
            return;
        }

        // =====================================================================
        // 3D СЦЕНА ОНЛАЙН-БОЯ / ПОЛИГОНА + 3D X-RAY КАМЕРА ПОПАДАНИЙ
        // =====================================================================
        int mapIdx = hasWorldSnap ? worldSnap.mapIdx : selectedMapChoice;
        const MapSpec& map = getMaps()[mapIdx];

        Vec3 camForward(
            -std::sin(camYaw) * std::cos(camPitch),
             std::sin(camPitch),
            -std::cos(camYaw) * std::cos(camPitch)
        );
        Vec3 camPos, camTarget;
        float fovDeg = 55.f;
        if (sniperMode) {
            fovDeg = 52.f / sniperZoom;
            camPos = Vec3(myX - std::sin(myTurretYaw) * 1.15f, myY + 1.94f, myZ - std::cos(myTurretYaw) * 1.15f);
            camTarget = camPos + camForward * 60.f;
        } else {
            Vec3 pivot(myX, myY + 2.55f, myZ);
            camPos = pivot - camForward * 9.4f + Vec3(0.f, 0.85f, 0.f);
            float gndH = getTerrainHeight(mapIdx, camPos.x, camPos.z) + 0.65f;
            if (camPos.y < gndH) camPos.y = gndH;
            camTarget = camPos + camForward * 60.f;
        }

        Vec3 sunDir = (mapIdx == 1) ? Vec3(-0.54f, 0.50f, -0.68f).normalized()
                                    : Vec3(0.58f, 0.52f, -0.62f).normalized();
        Vec3 sunCol = (mapIdx == 1) ? Vec3(1.06f, 0.90f, 0.74f) : Vec3(1.08f, 0.96f, 0.82f);
        Vec3 shadowFocus(myX, myY, myZ);
        Mat4 lightVP = Mat4::lookAtLH(shadowFocus + sunDir * 120.f, shadowFocus, Vec3(0, 1, 0)) *
                       Mat4::orthoLH(115.f, 115.f, 10.f, 280.f);
        Mat4 viewMat = Mat4::lookAtLH(camPos, camTarget, Vec3(0, 1, 0));
        Mat4 projMat = Mat4::perspectiveFovLH(deg2rad(fovDeg), aspect, 0.35f, 880.f);
        Mat4 mainVP  = viewMat * projMat;

        CBFrame fb{};
        fb.viewProj      = mainVP;
        fb.lightViewProj = lightVP;
        fb.camPosTime    = Vec4(camPos.x, camPos.y, camPos.z, (float)appTimeSec);
        fb.sunDirQuality = Vec4(sunDir.x, sunDir.y, sunDir.z, gfx.shadowsEnabled ? 1.f : 0.f);
        fb.sunColorExposure = Vec4(sunCol.x * 4.1f, sunCol.y * 4.1f, sunCol.z * 4.1f, 1.10f);
        fb.skyTopFogStart   = Vec4(map.skyTop.x, map.skyTop.y, map.skyTop.z, 165.f);
        fb.skyHorizFogEnd   = Vec4(map.skyHorizon.x, map.skyHorizon.y, map.skyHorizon.z, 520.f);
        fb.groundAmbMode    = Vec4(map.groundColor.x * 0.55f, map.groundColor.y * 0.55f, map.groundColor.z * 0.55f,
                                   (sniperMode && thermalMode) ? 1.f : 0.f);

        auto drawBattleWorld = [&](bool shadowPass) {
            if (!shadowPass) {
                if (!(sniperMode && thermalMode)) {
                    CBObject skyObj{};
                    skyObj.world = Mat4::translation(camPos.x, 0.f, camPos.z);
                    skyObj.tintAlpha = Vec4(1, 1, 1, 1);
                    gfx.drawMesh(envAssets.skyDome, skyObj);
                }
                CBObject terrObj{};
                terrObj.world = Mat4::identity();
                terrObj.tintAlpha = Vec4(1, 1, 1, 1);
                gfx.drawMesh(envAssets.mapTerrain[mapIdx], terrObj);
            }

            if (hasWorldSnap) {
                for (int i = 0; i < MAX_NET_TANKS; ++i) {
                    const NetTankSnap& ts = worldSnap.tanks[i];
                    if (!ts.active) continue;
                    // В снайперском прицеле скрываем собственный корпус, чтобы не перекрывал оптику
                    if (!shadowPass && sniperMode && i == mySlot) continue;

                    int cIdx = (i == mySlot) ? profile.vehicleCamo[ts.vehicleIdx] : (ts.team == 0 ? 1 : 3);
                    float burnt = ts.alive ? 0.f : 0.88f;
                    float rec = (i == mySlot) ? gunRecoil : 0.f;
                    float hotFlag = ts.alive ? 1.0f : 0.0f;
                    drawArticulatedTank(ts.vehicleIdx, renderTankPos[i], renderHullYaw[i], renderTurYaw[i],
                                        renderGunPitch[i], cIdx, hotFlag, burnt, rec, true);
                }
            }

            // Оторванные взрывом БК башни
            for (const auto& ft : flyingTurrets) {
                CBObject tobj{};
                tobj.world = Mat4::rotationX(ft.rot.x) * Mat4::rotationY(ft.rot.y) * Mat4::translation(ft.pos.x, ft.pos.y, ft.pos.z);
                tobj.tintAlpha = Vec4(0.18f, 0.18f, 0.18f, 1.f);
                tobj.armorParams = Vec4(200, 100, 80, 0.92f);
                gfx.drawMesh(vehModels[ft.vehIdx % vehModels.size()].turret, tobj);
            }
        };

        if (gfx.shadowsEnabled) {
            gfx.beginShadowPass(fb);
            drawBattleWorld(true);
        }
        gfx.beginMainPass(fb, thermalMode && sniperMode ? Vec4(0.02f, 0.05f, 0.04f, 1.f)
                                                        : Vec4(map.skyHorizon.x * 0.85f, map.skyHorizon.y * 0.85f, map.skyHorizon.z * 0.85f, 1.f));
        drawBattleWorld(false);

        // Кольца точек захвата [A/B/C], трассеры, управляемые ПТУР и объёмный дым/взрывы
        gfx.setAlphaBlend(true);
        if (!map.isPoligon && hasWorldSnap) {
            for (size_t z = 0; z < map.zones.size() && z < 3; ++z) {
                const auto& zn = map.zones[z];
                float zy = getTerrainHeight(mapIdx, zn.x, zn.z);
                int own = worldSnap.zoneOwner[z];
                Vec4 zCol = (own == myTeam) ? Vec4(0.22f, 0.66f, 1.0f, 0.85f)
                          : (own >= 0 ? Vec4(1.0f, 0.26f, 0.22f, 0.85f) : Vec4(0.95f, 0.95f, 0.95f, 0.78f));
                CBObject zo{};
                zo.world = Mat4::scaling(zn.radius, 1.f, zn.radius) * Mat4::translation(zn.x, zy, zn.z);
                zo.tintAlpha = zCol;
                gfx.drawMesh(envAssets.zoneRing, zo);
            }
        }

        // Летящие снаряды-трассеры и ПТУР
        for (const auto& tr : tracers) {
            Vec3 dir = tr.vel.normalized();
            float yaw = std::atan2(-dir.x, -dir.z);
            float pitch = std::asin(clampf(dir.y, -1.f, 1.f));
            CBObject to{};
            to.world = Mat4::scaling(tr.scale, tr.scale, tr.scale) *
                       Mat4::rotationX(pitch) * Mat4::rotationY(yaw) *
                       Mat4::translation(tr.pos.x, tr.pos.y, tr.pos.z);
            to.tintAlpha = tr.color;
            gfx.drawMesh(envAssets.unitTracer, to);
        }
        if (hasWorldSnap) {
            for (const auto& at : worldSnap.atgms) {
                if (!at.active) continue;
                CBObject ao{};
                ao.world = Mat4::scaling(0.32f, 0.32f, 0.32f) * Mat4::translation(at.x, at.y, at.z);
                ao.tintAlpha = Vec4(1.0f, 0.48f, 0.14f, 0.98f);
                gfx.drawMesh(envAssets.unitSphere, ao);
            }
        }
        // Клубы дыма и огненные шары взрывов
        for (const auto& pf : puffs) {
            float fade = 1.f - clampf(pf.age / std::max(0.01f, pf.life), 0.f, 1.f);
            CBObject po{};
            po.world = Mat4::scaling(pf.radius, pf.radius * 0.88f, pf.radius) * Mat4::translation(pf.pos.x, pf.pos.y, pf.pos.z);
            po.tintAlpha = Vec4(pf.color.x, pf.color.y, pf.color.z, pf.color.w * fade);
            gfx.drawMesh(envAssets.unitSphere, po);
        }
        gfx.setAlphaBlend(false);

        // =====================================================================
        // 3D КАМЕРА ПОПАДАНИЙ В ПРАВОМ ВЕРХНЕМ УГЛУ (КАК В WAR THUNDER!)
        // =====================================================================
        if (hitCamTimer > 0.f && activeHitCam.targetVehIdx < vehModels.size()) {
            // Сначала рисуем тёмную подложку окна Hit Cam через 2D UI, затем рендерим поверх 3D-танк в рентгене!
            ui.begin();
            float hcW = 336.f, hcH = 224.f;
            float hcX = gfx.width - hcW - 18.f;
            float hcY = 58.f;
            ui.drawPanel(hcX, hcY, hcW, hcH, Color4::rgba(9, 15, 22, 0.90f), Color4::rgba(6, 10, 16, 0.94f),
                         activeHitCam.result == HIT_KILL ? Color4::rgba(235, 64, 52, 0.95f) : Color4::rgba(76, 168, 230, 0.85f));
            ui.flush(gfx.width, gfx.height);

            Vec3 hitPt(activeHitCam.localHit[0], activeHitCam.localHit[1], activeHitCam.localHit[2]);
            Vec3 shDir = Vec3(activeHitCam.localDir[0], activeHitCam.localDir[1], activeHitCam.localDir[2]).normalized();
            float orbit = (5.4f - hitCamTimer) * 0.35f;
            Vec3 subCamPos = hitPt * 0.35f + Vec3(std::cos(orbit) * 4.8f + (hitPt.x >= 0 ? 1.8f : -1.8f), 2.35f, -4.6f);
            Mat4 subVP = Mat4::lookAtLH(subCamPos, Vec3(0.f, 1.15f, 0.f), Vec3(0, 1, 0)) *
                         Mat4::perspectiveFovLH(deg2rad(42.f), hcW / hcH, 0.2f, 40.f);

            CBFrame subFb = fb;
            subFb.viewProj = subVP;
            subFb.camPosTime = Vec4(subCamPos.x, subCamPos.y, subCamPos.z, (float)appTimeSec);
            subFb.sunDirQuality.w = 0.f;
            subFb.groundAmbMode.w = 0.f;

            gfx.beginSubViewport3D(hcX + 2.f, hcY + 26.f, hcW - 4.f, hcH - 64.f, subFb);

            // Внутренние модули вражеского танка
            CBObject xm{};
            xm.world = Mat4::identity();
            xm.tintAlpha = Vec4(1, 1, 1, 1);
            gfx.drawMesh(vehModels[activeHitCam.targetVehIdx].xrayModules, xm);

            // Анимированный снаряд и пучок вторичных осколков внутри пробитой брони!
            float animT = std::fmod((5.4f - hitCamTimer) * 1.45f, 1.5f);
            Vec3 shellAnimPos = hitPt - shDir * (1.8f - std::min(1.0f, animT) * 1.8f);
            float shYaw = std::atan2(-shDir.x, -shDir.z);
            float shPitch = std::asin(clampf(shDir.y, -1.f, 1.f));
            CBObject shObj{};
            shObj.world = Mat4::scaling(0.55f, 0.55f, 0.45f) * Mat4::rotationX(shPitch) * Mat4::rotationY(shYaw) *
                          Mat4::translation(shellAnimPos.x, shellAnimPos.y, shellAnimPos.z);
            shObj.tintAlpha = Vec4(1.0f, 0.85f, 0.22f, 1.f);
            gfx.drawMesh(envAssets.unitTracer, shObj);

            if (animT >= 0.95f && activeHitCam.result >= HIT_SPALL && activeHitCam.result != HIT_SPAWN_PROTECT) {
                float spallR = (animT - 0.95f) * 1.65f;
                CBObject spObj{};
                spObj.world = Mat4::scaling(spallR, spallR, spallR) *
                              Mat4::translation(hitPt.x + shDir.x * spallR * 0.9f, hitPt.y + shDir.y * spallR * 0.9f, hitPt.z + shDir.z * spallR * 0.9f);
                spObj.tintAlpha = Vec4(1.0f, 0.36f, 0.12f, 0.88f);
                gfx.drawMesh(envAssets.unitSphere, spObj);
            }

            gfx.setAlphaBlend(true);
            drawArticulatedTank(activeHitCam.targetVehIdx, Vec3(0, 0, 0), 0.f, 0.f, 0.f, 0, 3.0f);
            gfx.setAlphaBlend(false);

            // Возвращаем полноэкранный вьюпорт для остального 2D HUD
            D3D11_VIEWPORT fullVp{0.f, 0.f, (float)gfx.width, (float)gfx.height, 0.f, 1.f};
            gfx.ctx->RSSetViewports(1, &fullVp);
        }

        // Записываем актуальную матрицу камеры для проекции маркеров и кругов прицела в 2D HUD
        lastBattleViewProj = mainVP;
    }

    Mat4 lastBattleViewProj = Mat4::identity();

    void addHit(int id, int param, float x, float y, float w, float h) {
        hitBoxes.push_back({id, param, x, y, w, h});
    }

    void renderUi2D() {
        hitBoxes.clear();
        ui.begin();
        const float W = (float)gfx.width;
        const float H = (float)gfx.height;

        if (appState == 0) {
            renderHangarUi(W, H);
        } else {
            renderBattleHud(W, H);
        }

        ui.flush(gfx.width, gfx.height);
    }

    void renderHangarUi(float W, float H) {
        const auto& nations = getNations();
        const auto& vehs    = getVehicles();
        const auto& maps    = getMaps();
        const auto& camos   = getCamouflages();
        int vIdx = currentVehIdx();
        const VehicleSpec& v = vehs[vIdx];

        // 1. ВЕРХНЯЯ ШАПКА АНГАРА (ЯДРО DAGOR ENGINE 6.5 • VROMFS + .BLK + daECS)
        const auto& dagKernel = dagor::RuntimeKernel::instance();
        ui.drawPanel(0, 0, W, 56, Color4::rgba(14, 18, 24, 0.94f), Color4::rgba(9, 12, 17, 0.96f), Color4::rgba(58, 70, 88, 0.65f));
        ui.drawText("STEEL FRONT", 20, 8, 20.5f, Color4::rgba(238, 242, 248));
        ui.drawText("DAGOR ENGINE 6.5 • VROMFS + .BLK", 20, 31, 11.2f, Color4::rgba(226, 178, 68));

        char srvBadge[180];
        std::snprintf(srvBadge, sizeof(srvBadge), "DAGOR (%d .BLK / %d daECS%s) • %s:%d (%.0f МС)",
                      dagKernel.loadedVehiclesBlk, dagKernel.entityMgr.templateCount(),
                      dagKernel.hotReloadCount > 0 ? " • ОБНОВЛЕНО!" : "",
                      settings.serverIp, settings.serverPort, netPingMs);
        ui.drawText(srvBadge, 232, 20, 12.2f, Color4::rgba(116, 216, 148));

        // Центральная красная кнопка «В БОЙ!» + выбор карты/режима под ней
        float bBtnW = 216.f, bBtnH = 44.f;
        float bBtnX = W * 0.5f - bBtnW * 0.5f, bBtnY = 6.f;
        bool hovBattle = (mouseX >= bBtnX && mouseX <= bBtnX + bBtnW && mouseY >= bBtnY && mouseY <= bBtnY + bBtnH);
        ui.drawPanel(bBtnX, bBtnY, bBtnW, bBtnH,
                     hovBattle ? Color4::rgba(232, 58, 44) : Color4::rgba(198, 38, 30),
                     hovBattle ? Color4::rgba(176, 26, 20) : Color4::rgba(138, 18, 15),
                     Color4::rgba(255, 148, 120, 0.9f));
        ui.drawTextCentered(selectedMapChoice == 3 ? "НА ПОЛИГОН!" : "В БОЙ!", W * 0.5f, 14, 24, Color4(1, 1, 1, 1));
        addHit(ACT_ENTER_BATTLE, selectedMapChoice, bBtnX, bBtnY, bBtnW, bBtnH);

        // Плашка выбора режима/карты под кнопкой «В БОЙ!»
        float mBarW = 360.f, mBarX = W * 0.5f - mBarW * 0.5f, mBarY = 60.f;
        bool hovMap = (mouseX >= mBarX && mouseX <= mBarX + mBarW && mouseY >= mBarY && mouseY <= mBarY + 28.f);
        ui.drawPanel(mBarX, mBarY, mBarW, 28.f,
                     hovMap ? Color4::rgba(28, 36, 48, 0.92f) : Color4::rgba(16, 22, 30, 0.88f),
                     Color4::rgba(12, 16, 22, 0.92f), Color4::rgba(92, 112, 138, 0.7f));
        char mapLabel[140];
        std::snprintf(mapLabel, sizeof(mapLabel), "▼  %s: %s", maps[selectedMapChoice].subtitle, maps[selectedMapChoice].name);
        ui.drawTextCentered(mapLabel, W * 0.5f, mBarY + 5.f, 13.5f, Color4::rgba(232, 238, 248));
        addHit(ACT_SELECT_MAP, (selectedMapChoice + 1) % 4, mBarX, mBarY, mBarW, 28.f);

        // Кнопка быстрого выезда на Полигон + кнопка живого моддинга Dagor .BLK (CDK)
        float polX = bBtnX + bBtnW + 10.f;
        ui.drawPanel(polX, 10.f, 124.f, 36.f, Color4::rgba(32, 44, 58, 0.95f), Color4::rgba(20, 28, 38, 0.95f), Color4::rgba(92, 136, 182, 0.8f));
        ui.drawTextCentered("ПРОБНЫЙ ВЫЕЗД", polX + 62.f, 19.f, 12.2f, Color4::rgba(210, 230, 252));
        addHit(ACT_ENTER_POLIGON, 3, polX, 10.f, 124.f, 36.f);

        float cdkX = polX + 132.f;
        ui.drawPanel(cdkX, 10.f, 118.f, 36.f, Color4::rgba(46, 38, 20, 0.95f), Color4::rgba(28, 22, 12, 0.95f), Color4::rgba(228, 182, 74, 0.85f));
        ui.drawTextCentered("CDK .BLK (F5)", cdkX + 59.f, 19.f, 12.0f, Color4::rgba(250, 218, 126));
        addHit(ACT_OPEN_BLK_CDK, 0, cdkX, 10.f, 118.f, 36.f);

        // Кнопка дерева исследований слева от «В БОЙ!»
        float ttX = bBtnX - 150.f;
        ui.drawPanel(ttX, 10.f, 138.f, 36.f,
                     showTechTree ? Color4::rgba(196, 142, 34) : Color4::rgba(32, 42, 54, 0.95f),
                     showTechTree ? Color4::rgba(142, 98, 18) : Color4::rgba(20, 26, 36, 0.95f),
                     Color4::rgba(228, 184, 78, 0.8f));
        ui.drawTextCentered("ИССЛЕДОВАНИЯ", ttX + 69.f, 19.f, 13, Color4(1, 1, 1, 1));
        addHit(ACT_TOGGLE_TECH_TREE, 0, ttX, 10.f, 138.f, 36.f);

        // Валюта аккаунта и кнопки звука/выхода справа
        char econStr[128];
        std::snprintf(econStr, sizeof(econStr), "%d СЛ     %d ОИ     %d ЗО", profile.silver, profile.rp, profile.gold);
        ui.drawTextRight(econStr, W - 122.f, 18.f, 15, Color4::rgba(244, 214, 118));

        ui.drawPanel(W - 108.f, 12.f, 44.f, 32.f, Color4::rgba(28, 34, 44), Color4::rgba(18, 22, 30), Color4::rgba(80, 95, 115));
        ui.drawTextCentered(sound.enabled ? "ЗВУК" : "ТИХО", W - 86.f, 20.f, 12, sound.enabled ? Color4::rgba(130, 230, 160) : Color4::rgba(180, 180, 190));
        addHit(ACT_TOGGLE_SOUND, 0, W - 108.f, 12.f, 44.f, 32.f);

        ui.drawPanel(W - 56.f, 12.f, 44.f, 32.f, Color4::rgba(92, 26, 26), Color4::rgba(58, 16, 16), Color4::rgba(190, 72, 72));
        ui.drawTextCentered("ВЫХ", W - 34.f, 20.f, 12, Color4(1, 1, 1, 1));
        addHit(ACT_EXIT_GAME, 0, W - 56.f, 12.f, 44.f, 32.f);

        // 2. ЛЕВАЯ КАРТОЧКА ТТХ, БРОНЕПРОБИТИЯ И КАМУФЛЯЖЕЙ (В СТИЛЕ WAR THUNDER)
        float cardX = 18.f, cardY = 72.f, cardW = 428.f, cardH = std::min(498.f, H - 220.f);
        ui.drawPanel(cardX, cardY, cardW, cardH, Color4::rgba(11, 16, 23, 0.90f), Color4::rgba(8, 11, 17, 0.94f), Color4::rgba(74, 92, 118, 0.78f));

        ui.drawNationFlag(v.nationIdx, cardX + 16.f, cardY + 14.f, 36.f, 22.f);
        ui.drawText(v.name, cardX + 60.f, cardY + 10.f, 18.0f, Color4::rgba(244, 248, 255));
        char subLine[128];
        std::snprintf(subLine, sizeof(subLine), "БР %s  •  Ранг %s  •  %s", v.br, v.rank, v.roleClass);
        ui.drawText(subLine, cardX + 60.f, cardY + 32.f, 12.2f, Color4::rgba(182, 198, 218));

        auto statRow = [&](float ry, const char* k, const char* val) {
            ui.drawText(k, cardX + 16.f, ry, 12.5f, Color4::rgba(158, 176, 198));
            ui.drawTextRight(val, cardX + cardW - 16.f, ry, 12.8f, Color4::rgba(238, 243, 250));
        };
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%d / %d / %d мм", v.turretArmor[0], v.turretArmor[1], v.turretArmor[2]);
        statRow(cardY + 62.f, "Броня башни (лоб / борт / корма):", buf);
        std::snprintf(buf, sizeof(buf), "%d / %d / %d мм", v.hullArmor[0], v.hullArmor[1], v.hullArmor[2]);
        statRow(cardY + 83.f, "Броня корпуса (ВЛД / борт / корма):", buf);
        std::snprintf(buf, sizeof(buf), "%s (%.1f с)", v.gunName, v.reloadSec);
        statRow(cardY + 104.f, "Вооружение / Перезарядка:", buf);
        std::snprintf(buf, sizeof(buf), "+%.0f / -%.0f км/ч  (%d л.с.)", v.fwdKmh, v.revKmh, v.engineHp);
        statRow(cardY + 125.f, "Макс. скорость / Двигатель:", buf);
        std::snprintf(buf, sizeof(buf), "%.0f°/с  •  ДЗ: %s", v.turretDegSec, v.eraName);
        statRow(cardY + 146.f, "Привод башни / Защита:", buf);

        // Выбор боеприпасов (1, 2, 3) и таблица бронепробития по дистанциям
        ui.fillRect(cardX + 14.f, cardY + 172.f, cardW - 28.f, 1.f, Color4(1, 1, 1, 0.12f));
        ui.drawText("БОЕКОМПЛЕКТ И БРОНЕПРОБИТИЕ (0°):", cardX + 16.f, cardY + 180.f, 12.5f, Color4::rgba(224, 184, 86));

        for (int s = 0; s < 3; ++s) {
            float sx = cardX + 16.f + s * 136.f;
            float sy = cardY + 202.f;
            bool sel = (selectedShellIdx == s);
            ui.drawPanel(sx, sy, 124.f, 38.f,
                         sel ? Color4::rgba(46, 78, 116, 0.96f) : Color4::rgba(22, 28, 38, 0.92f),
                         sel ? Color4::rgba(28, 48, 76, 0.96f)  : Color4::rgba(16, 20, 28, 0.92f),
                         sel ? Color4::rgba(116, 188, 255) : Color4::rgba(62, 76, 94));
            ui.drawTextCentered(v.shells[s].code, sx + 62.f, sy + 4.f, 12.4f, Color4(1, 1, 1, 1));
            char pStr[32];
            std::snprintf(pStr, sizeof(pStr), "%d мм", v.shells[s].basePenMM);
            ui.drawTextCentered(pStr, sx + 62.f, sy + 20.f, 12.0f, Color4::rgba(250, 208, 102));
            addHit(ACT_SELECT_SHELL, s, sx, sy, 124.f, 38.f);
        }

        const ShellSpec& curSh = v.shells[selectedShellIdx];
        int m0[3], m200[3], m500[3];
        curSh.getPenTable(m0, m200, m500);
        std::snprintf(buf, sizeof(buf), "%s  •  %d м/с  •  10м: %d  |  200м: %d  |  500м: %d мм",
                      curSh.label, (int)curSh.speedMs, m0[0], m200[0], m500[0]);
        ui.drawText(buf, cardX + 16.f, cardY + 247.f, 11.8f, Color4::rgba(196, 212, 232));

        // Свотчи камуфляжа (6 штук)
        ui.fillRect(cardX + 14.f, cardY + 272.f, cardW - 28.f, 1.f, Color4(1, 1, 1, 0.12f));
        std::snprintf(buf, sizeof(buf), "КАМУФЛЯЖ: %s", camos[profile.vehicleCamo[vIdx]].name);
        ui.drawText(buf, cardX + 16.f, cardY + 280.f, 12.2f, Color4::rgba(224, 184, 86));

        for (int c = 0; c < (int)camos.size(); ++c) {
            float cx = cardX + 16.f + c * 67.6f;
            float cy = cardY + 303.f;
            Vec3 sw = camos[c].useVehicleBase ? v.baseColor : camos[c].c0;
            ui.fillRect(cx, cy, 58.f, 26.f, Color4(sw.x, sw.y, sw.z, 1.f));
            if (!camos[c].useVehicleBase) {
                ui.fillRect(cx + 29.f, cy, 29.f, 13.f, Color4(camos[c].c1.x, camos[c].c1.y, camos[c].c1.z, 1.f));
                ui.fillRect(cx + 29.f, cy + 13.f, 29.f, 13.f, Color4(camos[c].c2.x, camos[c].c2.y, camos[c].c2.z, 1.f));
            }
            bool selC = (profile.vehicleCamo[vIdx] == c);
            ui.drawRectOutline(cx, cy, 58.f, 26.f, selC ? 2.f : 1.f, selC ? Color4::rgba(255, 214, 92) : Color4(1,1,1,0.3f));
            addHit(ACT_SELECT_CAMO, c, cx, cy, 58.f, 26.f);
        }

        // Режимы анализа брони / рентгена / смены сцены ангара
        ui.fillRect(cardX + 14.f, cardY + 340.f, cardW - 28.f, 1.f, Color4(1, 1, 1, 0.12f));
        const char* inspNames[3] = {"ОБЫЧНЫЙ ВИД", "АНАЛИЗ БРОНИ", "РЕНТГЕН X-RAY"};
        for (int m = 0; m < 3; ++m) {
            float mx = cardX + 16.f + m * 136.f;
            float my = cardY + 350.f;
            bool act = (inspectorMode == m);
            ui.drawPanel(mx, my, 124.f, 30.f,
                         act ? Color4::rgba(52, 92, 132) : Color4::rgba(22, 28, 38),
                         act ? Color4::rgba(32, 58, 86)  : Color4::rgba(15, 19, 26),
                         act ? Color4::rgba(120, 200, 255) : Color4::rgba(64, 78, 96));
            ui.drawTextCentered(inspNames[m], mx + 62.f, my + 7.f, 11.8f, Color4(1, 1, 1, 1));
            addHit(ACT_SET_INSPECTOR, m, mx, my, 124.f, 30.f);
        }

        ui.drawPanel(cardX + 16.f, cardY + 390.f, cardW - 32.f, 30.f, Color4::rgba(26, 34, 46), Color4::rgba(18, 24, 34), Color4::rgba(76, 96, 122));
        ui.drawTextCentered(hangarBunkerMode ? "ЛОКАЦИЯ АНГАРА: ЗАЩИЩЁННЫЙ БУНКЕР (СМЕНИТЬ)" : "ЛОКАЦИЯ АНГАРА: ПОЛЕВАЯ ВОЕННАЯ БАЗА (СМЕНИТЬ)",
                            cardX + cardW * 0.5f, cardY + 397.f, 11.8f, Color4::rgba(210, 226, 245));
        addHit(ACT_TOGGLE_HANGAR_SCENE, 0, cardX + 16.f, cardY + 390.f, cardW - 32.f, 30.f);

        // Кнопка исследования и покупки, если машина ещё закрыта
        if (!profile.isUnlocked(vIdx)) {
            bool canBuy = (profile.rp >= v.costRp && profile.silver >= v.costSilver);
            ui.drawPanel(cardX + 16.f, cardY + 432.f, cardW - 32.f, 44.f,
                         canBuy ? Color4::rgba(42, 138, 68) : Color4::rgba(82, 42, 42),
                         canBuy ? Color4::rgba(24, 92, 44)  : Color4::rgba(54, 26, 26),
                         Color4::rgba(150, 235, 168));
            std::snprintf(buf, sizeof(buf), "ИССЛЕДОВАТЬ И КУПИТЬ (%d ОИ + %d СЛ)", v.costRp, v.costSilver);
            ui.drawTextCentered(buf, cardX + cardW * 0.5f, cardY + 445.f, 12.5f, Color4(1, 1, 1, 1));
            addHit(ACT_UNLOCK_VEHICLE, vIdx, cardX + 16.f, cardY + 432.f, cardW - 32.f, 44.f);
        } else {
            ui.drawTextCentered("✓ МАШИНА В СТРОЮ • ЛКМ/ПКМ — КАМЕРА, КОЛЕСО — ЗУМ",
                                cardX + cardW * 0.5f, cardY + 446.f, 12.0f, Color4::rgba(128, 224, 154));
        }

        // 3. НИЖНЯЯ ПАНЕЛЬ НАЦИЙ (7 СТРАН С ФЛАГАМИ) И СЛОТЫ ЭКИПАЖЕЙ (КАК В WAR THUNDER)
        float botH = 128.f;
        float botY = H - botH;
        ui.drawPanel(0, botY, W, botH, Color4::rgba(12, 16, 22, 0.94f), Color4::rgba(8, 11, 16, 0.98f), Color4::rgba(58, 72, 92, 0.7f));

        // Полоска 7 наций
        float natW = 136.f, natGap = 8.f;
        float totalNatW = nations.size() * natW + (nations.size() - 1) * natGap;
        float natStartX = W * 0.5f - totalNatW * 0.5f;
        for (int n = 0; n < (int)nations.size(); ++n) {
            float nx = natStartX + n * (natW + natGap);
            float ny = botY + 8.f;
            bool sel = (profile.selectedNation == n);
            ui.drawPanel(nx, ny, natW, 32.f,
                         sel ? Color4::rgba(52, 74, 104, 0.98f) : Color4::rgba(20, 26, 36, 0.92f),
                         sel ? Color4::rgba(30, 44, 66, 0.98f)  : Color4::rgba(14, 18, 25, 0.92f),
                         sel ? Color4::rgba(238, 192, 82) : Color4::rgba(62, 76, 96));
            ui.drawNationFlag(n, nx + 8.f, ny + 7.f, 26.f, 18.f);
            ui.drawText(nations[n].name, nx + 40.f, ny + 8.f, 13, sel ? Color4::rgba(255, 232, 162) : Color4::rgba(214, 222, 235));
            addHit(ACT_SELECT_NATION, n, nx, ny, natW, 32.f);
        }

        // Слоты боевых машин выбранной нации
        std::vector<int> natVehs;
        for (int i = 0; i < (int)vehs.size(); ++i)
            if (vehs[i].nationIdx == profile.selectedNation) natVehs.push_back(i);

        float slotW = 196.f, slotH = 70.f, slotGap = 12.f;
        float totalSlotW = natVehs.size() * slotW + (natVehs.size() - 1) * slotGap;
        float slotStartX = W * 0.5f - totalSlotW * 0.5f;
        for (size_t k = 0; k < natVehs.size(); ++k) {
            int idx = natVehs[k];
            const VehicleSpec& sv = vehs[idx];
            float sx = slotStartX + k * (slotW + slotGap);
            float sy = botY + 48.f;
            bool sel = (vIdx == idx);
            bool unl = profile.isUnlocked(idx);

            ui.drawPanel(sx, sy, slotW, slotH,
                         sel ? Color4::rgba(44, 68, 98, 0.96f) : (unl ? Color4::rgba(22, 30, 42, 0.94f) : Color4::rgba(22, 20, 24, 0.90f)),
                         sel ? Color4::rgba(24, 38, 58, 0.98f) : Color4::rgba(14, 18, 26, 0.96f),
                         sel ? Color4::rgba(248, 198, 78) : (unl ? Color4::rgba(78, 98, 124) : Color4::rgba(112, 62, 62)));

            // Мини-силуэт танка на карточке экипажа
            Vec3 pCol = sv.baseColor;
            ui.fillRect(sx + 12.f, sy + 36.f, 48.f, 14.f, Color4(pCol.x, pCol.y, pCol.z, 0.95f));
            ui.fillRect(sx + 22.f, sy + 25.f, 26.f, 12.f, Color4(pCol.x * 1.1f, pCol.y * 1.1f, pCol.z * 1.1f, 0.95f));
            ui.fillRect(sx + 46.f, sy + 29.f, 24.f, 3.5f, Color4::rgba(180, 190, 200));

            ui.drawText(sv.shortName, sx + 12.f, sy + 6.f, 14.5f, Color4::rgba(244, 248, 255));
            std::snprintf(buf, sizeof(buf), "БР %s", sv.br);
            ui.drawTextRight(buf, sx + slotW - 10.f, sy + 7.f, 13, Color4::rgba(244, 202, 92));
            ui.drawTextRight(unl ? "В СТРОЮ" : "ЗАКРЫТО", sx + slotW - 10.f, sy + 46.f, 11.5f,
                             unl ? Color4::rgba(118, 224, 148) : Color4::rgba(228, 116, 108));
            addHit(ACT_SELECT_VEHICLE, idx, sx, sy, slotW, slotH);
        }

        // 4. МОДАЛЬНОЕ ОКНО ДЕРЕВА ИССЛЕДОВАНИЙ БРОНЕТЕХНИКИ
        if (showTechTree) {
            float mw = 780.f, mh = 430.f;
            float mx = W * 0.5f - mw * 0.5f, my = H * 0.5f - mh * 0.5f - 20.f;
            ui.drawPanel(mx, my, mw, mh, Color4::rgba(12, 17, 25, 0.97f), Color4::rgba(8, 12, 18, 0.99f), Color4::rgba(224, 182, 78));
            std::snprintf(buf, sizeof(buf), "ДЕРЕВО ИССЛЕДОВАНИЙ БРОНЕТЕХНИКИ — %s", nations[profile.selectedNation].name);
            ui.drawText(buf, mx + 22.f, my + 16.f, 18, Color4::rgba(246, 214, 118));
            ui.drawPanel(mx + mw - 112.f, my + 12.f, 94.f, 30.f, Color4::rgba(98, 32, 32), Color4::rgba(64, 18, 18), Color4::rgba(210, 90, 90));
            ui.drawTextCentered("ЗАКРЫТЬ", mx + mw - 65.f, my + 19.f, 12.5f, Color4(1, 1, 1, 1));
            addHit(ACT_TOGGLE_TECH_TREE, 0, mx + mw - 112.f, my + 12.f, 94.f, 30.f);

            for (size_t k = 0; k < natVehs.size(); ++k) {
                int idx = natVehs[k];
                const VehicleSpec& tv = vehs[idx];
                float ry = my + 66.f + k * 84.f;
                bool unl = profile.isUnlocked(idx);
                ui.drawPanel(mx + 26.f, ry, mw - 52.f, 70.f,
                             unl ? Color4::rgba(24, 38, 54, 0.95f) : Color4::rgba(24, 24, 32, 0.95f),
                             Color4::rgba(14, 20, 30, 0.95f),
                             unl ? Color4::rgba(92, 186, 124) : Color4::rgba(92, 104, 124));
                ui.drawText(tv.name, mx + 44.f, ry + 10.f, 16.5f, Color4(1, 1, 1, 1));
                std::snprintf(buf, sizeof(buf), "БР %s • Ранг %s • %s • Лобовая башни %d мм • %s (%d мм)",
                              tv.br, tv.rank, tv.roleClass, tv.turretArmor[0], tv.shells[0].code, tv.shells[0].basePenMM);
                ui.drawText(buf, mx + 44.f, ry + 36.f, 12.2f, Color4::rgba(182, 198, 218));

                if (unl) {
                    ui.drawPanel(mx + mw - 196.f, ry + 17.f, 150.f, 36.f, Color4::rgba(38, 86, 126), Color4::rgba(24, 56, 86), Color4::rgba(120, 196, 255));
                    ui.drawTextCentered("ВЫБРАТЬ В АНГАР", mx + mw - 121.f, ry + 26.f, 12.5f, Color4(1, 1, 1, 1));
                    addHit(ACT_SELECT_VEHICLE, idx, mx + mw - 196.f, ry + 17.f, 150.f, 36.f);
                } else {
                    bool afford = (profile.rp >= tv.costRp && profile.silver >= tv.costSilver);
                    ui.drawPanel(mx + mw - 236.f, ry + 17.f, 190.f, 36.f,
                                 afford ? Color4::rgba(42, 136, 68) : Color4::rgba(76, 44, 44),
                                 afford ? Color4::rgba(24, 88, 42)  : Color4::rgba(48, 28, 28),
                                 Color4::rgba(164, 232, 178));
                    std::snprintf(buf, sizeof(buf), "ОТКРЫТЬ: %d ОИ + %d СЛ", tv.costRp, tv.costSilver);
                    ui.drawTextCentered(buf, mx + mw - 141.f, ry + 26.f, 12.f, Color4(1, 1, 1, 1));
                    addHit(ACT_UNLOCK_VEHICLE, idx, mx + mw - 236.f, ry + 17.f, 190.f, 36.f);
                }
            }
        }
    }

    void renderBattleHud(float W, float H) {
        int mapIdx = hasWorldSnap ? worldSnap.mapIdx : selectedMapChoice;
        const MapSpec& map = getMaps()[mapIdx];
        const auto& vehs = getVehicles();
        char buf[160];

        // 1. СНАЙПЕРСКАЯ ПРИЦЕЛЬНАЯ СЕТКА (SHIFT / ПКМ) И ДВА КРУГА WAR THUNDER
        if (sniperMode) {
            // Тёмная виньетка оптики наводчика + баллистическая шкала тысячных и поправок по дальности
            float rad = std::min(W, H) * 0.44f;
            float cx = W * 0.5f, cy = H * 0.5f;
            ui.fillRect(0, 0, cx - rad, H, Color4(0, 0, 0, 0.88f));
            ui.fillRect(cx + rad, 0, W - (cx + rad), H, Color4(0, 0, 0, 0.88f));
            ui.fillRect(cx - rad, 0, rad * 2.f, std::max(0.f, cy - rad), Color4(0, 0, 0, 0.88f));
            ui.fillRect(cx - rad, cy + rad, rad * 2.f, std::max(0.f, H - (cy + rad)), Color4(0, 0, 0, 0.88f));
            ui.drawCircleOutline(cx, cy, rad, 2.5f, thermalMode ? Color4::rgba(110, 255, 165, 0.85f) : Color4::rgba(28, 32, 36, 0.95f), 64);

            Color4 retCol = thermalMode ? Color4::rgba(130, 255, 175, 0.92f) : Color4::rgba(24, 26, 28, 0.92f);
            ui.drawLine(cx - rad * 0.82f, cy, cx - 12.f, cy, 1.4f, retCol);
            ui.drawLine(cx + 12.f, cy, cx + rad * 0.82f, cy, 1.4f, retCol);
            ui.drawLine(cx, cy + 10.f, cx, cy + rad * 0.75f, 1.4f, retCol);
            // Центральный шеврон прицела «Сосна-У» / 1Г46
            ui.drawLine(cx - 9.f, cy + 8.f, cx, cy, 1.8f, Color4::rgba(235, 58, 46));
            ui.drawLine(cx + 9.f, cy + 8.f, cx, cy, 1.8f, Color4::rgba(235, 58, 46));

            for (int m = 1; m <= 6; ++m) {
                float off = m * 36.f;
                ui.drawLine(cx - off, cy - 5.f, cx - off, cy + 5.f, 1.2f, retCol);
                ui.drawLine(cx + off, cy - 5.f, cx + off, cy + 5.f, 1.2f, retCol);
                float dropY = cy + m * 26.f;
                ui.drawLine(cx - 10.f, dropY, cx + 10.f, dropY, 1.2f, retCol);
                std::snprintf(buf, sizeof(buf), "%d", m * 4);
                ui.drawText(buf, cx + 15.f, dropY - 7.f, 11.f, retCol, false);
            }

            std::snprintf(buf, sizeof(buf), "КРАТНОСТЬ %.1fX  •  %s  •  ДАЛЬНОМЕР [R]: %d М",
                          sniperZoom, thermalMode ? "ТВП ВКЛ [N]" : "ДНЕВНОЙ КАНАЛ [N — ТВП]",
                          measuredRangeM > 0 ? measuredRangeM : int((actualBoreWorld - Vec3(myX, myY, myZ)).length() * 1.4f));
            ui.drawTextCentered(buf, cx, cy - rad + 18.f, 13.5f, Color4::rgba(128, 246, 168));
        }

        // Два прицельных круга в стиле War Thunder (большой круг команды мыши + малый круг реального ствола!)
        Vec2 cmdScr, boreScr;
        if (projectWorldToScreen(commandAimWorld, lastBattleViewProj, W, H, cmdScr)) {
            ui.drawCircleOutline(cmdScr.x, cmdScr.y, 19.f, 1.8f, Color4(1.f, 1.f, 1.f, 0.85f), 32);
        }
        if (projectWorldToScreen(actualBoreWorld, lastBattleViewProj, W, H, boreScr)) {
            bool ready = (hasWorldSnap && mySlot >= 0 && worldSnap.tanks[mySlot].reloadTimer <= 0.01f);
            Color4 bCol = ready ? Color4::rgba(116, 245, 138, 0.95f) : Color4::rgba(248, 86, 68, 0.95f);
            ui.drawCircleOutline(boreScr.x, boreScr.y, 9.5f, 1.8f, bCol, 24);
            ui.fillCircle(boreScr.x, boreScr.y, 2.2f, bCol, 10);
        }

        // 2. 3D-МАРКЕРЫ СОЮЗНИКОВ, РАЗВЕДАННЫХ ЦЕЛЕЙ И ТОЧЕК [A/B/C] НАД ПОЛЕМ БОЯ
        if (hasWorldSnap) {
            for (size_t z = 0; z < map.zones.size() && z < 3; ++z) {
                Vec3 zp(map.zones[z].x, getTerrainHeight(mapIdx, map.zones[z].x, map.zones[z].z) + 5.5f, map.zones[z].z);
                Vec2 zs;
                if (projectWorldToScreen(zp, lastBattleViewProj, W, H, zs)) {
                    int own = worldSnap.zoneOwner[z];
                    Color4 zc = (own == myTeam) ? Color4::rgba(68, 168, 255) : (own >= 0 ? Color4::rgba(245, 76, 68) : Color4::rgba(235, 238, 244));
                    ui.drawPanel(zs.x - 15.f, zs.y - 15.f, 30.f, 30.f, Color4::rgba(12, 18, 26, 0.85f), Color4::rgba(8, 12, 18, 0.9f), zc);
                    char zCode[2] = {map.zones[z].id, 0};
                    ui.drawTextCentered(zCode, zs.x, zs.y - 9.f, 16, zc);
                }
            }

            for (int i = 0; i < MAX_NET_TANKS; ++i) {
                const NetTankSnap& ts = worldSnap.tanks[i];
                if (!ts.active || !ts.alive || i == mySlot) continue;
                Vec3 wp = renderTankPos[i] + Vec3(0.f, 2.85f, 0.f);
                float distM = (wp - Vec3(myX, myY, myZ)).length() * 1.4f;
                if (ts.team != myTeam && !map.isPoligon) {
                    // Враги подсвечиваются при прямой видимости
                    if (checkSegmentBlocked(map, Vec3(myX, myY + 2.1f, myZ), wp)) continue;
                }
                Vec2 sc;
                if (projectWorldToScreen(wp, lastBattleViewProj, W, H, sc)) {
                    bool ally = (ts.team == myTeam);
                    Color4 mc = ally ? Color4::rgba(82, 178, 255) : Color4::rgba(248, 78, 68);
                    std::snprintf(buf, sizeof(buf), "%s  [%.0f м]", vehs[ts.vehicleIdx].shortName, distM);
                    ui.drawTextCentered(buf, sc.x, sc.y - 22.f, 12.5f, mc);
                    float hpFrac = clampf(float(ts.hp) / std::max<int>(1, ts.maxHp), 0.f, 1.f);
                    ui.fillRect(sc.x - 28.f, sc.y - 5.f, 56.f, 5.f, Color4(0, 0, 0, 0.7f));
                    ui.fillRect(sc.x - 27.f, sc.y - 4.f, 54.f * hpFrac, 3.f, mc);
                }
            }
        }

        // 3. ВЕРХНЯЯ ПОЛОСА ОЧКОВ КОМАНД И ТОЧЕК [A/B/C] (КАК В WAR THUNDER!)
        if (!map.isPoligon && hasWorldSnap) {
            float topW = 520.f, topX = W * 0.5f - topW * 0.5f;
            ui.drawPanel(topX, 8.f, topW, 40.f, Color4::rgba(12, 16, 24, 0.88f), Color4::rgba(8, 11, 17, 0.92f), Color4::rgba(62, 78, 98, 0.7f));

            uint16_t myT = (myTeam == 0) ? worldSnap.ticketsBlue : worldSnap.ticketsRed;
            uint16_t enT = (myTeam == 0) ? worldSnap.ticketsRed  : worldSnap.ticketsBlue;
            float myF = clampf(myT / 2500.f, 0.f, 1.f);
            float enF = clampf(enT / 2500.f, 0.f, 1.f);

            ui.fillRect(topX + 12.f, 16.f, 176.f, 12.f, Color4::rgba(22, 30, 42));
            ui.fillRect(topX + 12.f + 176.f * (1.f - myF), 16.f, 176.f * myF, 12.f, Color4::rgba(58, 152, 245));
            ui.fillRect(topX + topW - 188.f, 16.f, 176.f, 12.f, Color4::rgba(42, 22, 24));
            ui.fillRect(topX + topW - 188.f, 16.f, 176.f * enF, 12.f, Color4::rgba(232, 64, 56));

            for (int z = 0; z < 3; ++z) {
                float zx = W * 0.5f - 46.f + z * 34.f;
                int own = worldSnap.zoneOwner[z];
                Color4 zc = (own == myTeam) ? Color4::rgba(58, 152, 245) : (own >= 0 ? Color4::rgba(232, 64, 56) : Color4::rgba(160, 170, 185));
                ui.drawPanel(zx, 12.f, 24.f, 22.f, zc, Color4(zc.r*0.6f, zc.g*0.6f, zc.b*0.6f, 1.f), Color4(1,1,1,0.7f));
                const char* zL[3] = {"A", "B", "C"};
                ui.drawTextCentered(zL[z], zx + 12.f, 15.f, 13, Color4(1,1,1,1));
            }
            int mm = worldSnap.timeLeftSec / 60, ss = worldSnap.timeLeftSec % 60;
            std::snprintf(buf, sizeof(buf), "%u          %02d:%02d          %u", myT, mm, ss, enT);
            ui.drawTextCentered(buf, W * 0.5f, 32.f, 11.5f, Color4::rgba(218, 226, 238));
        } else {
            ui.drawPanel(W * 0.5f - 230.f, 8.f, 460.f, 34.f, Color4::rgba(12, 18, 26, 0.88f), Color4::rgba(8, 12, 18, 0.92f), Color4::rgba(88, 138, 188));
            ui.drawTextCentered("ТАНКОВЫЙ СТРЕЛКОВЫЙ ПОЛИГОН • ESC — МЕНЮ / СМЕНА ТАНКА", W * 0.5f, 16.f, 13.f, Color4::rgba(220, 236, 255));
        }

        // Сетевой статус и FPS слева вверху
        std::snprintf(buf, sizeof(buf), "FPS: %.0f  •  ПИНГ: %.0f мс  •  ИГРОКОВ ОНЛАЙН: %u", fpsSmoothed, netPingMs, onlineHumans);
        ui.drawText(buf, 16.f, 12.f, 12.5f, Color4::rgba(182, 206, 226));

        // 4. ПОДПИСИ ПОВЕРХ ОКНА 3D КАМЕРЫ ПОПАДАНИЙ (ПРАВЫЙ ВЕРХНИЙ УГОЛ)
        if (hitCamTimer > 0.f) {
            float hcW = 336.f, hcH = 224.f;
            float hcX = W - hcW - 18.f, hcY = 58.f;
            static const char* resTitles[] = {
                "РИКОШЕТ", "НЕ ПРОБИЛ БРОНЮ", "ОСКОЛОЧНОЕ ВОЗДЕЙСТВИЕ", "ПРОБИТИЕ БРОНИ",
                "КРИТИЧЕСКОЕ ПОВРЕЖДЕНИЕ!", "ТАНК УНИЧТОЖЕН!", "ЗАЩИТА ВОЗРОЖДЕНИЯ"
            };
            Color4 rCol = (activeHitCam.result >= HIT_CRIT && activeHitCam.result != HIT_SPAWN_PROTECT)
                        ? Color4::rgba(255, 92, 74)
                        : (activeHitCam.result >= HIT_SPALL ? Color4::rgba(118, 238, 148) : Color4::rgba(220, 226, 235));
            ui.drawText(resTitles[activeHitCam.result % 7], hcX + 10.f, hcY + 5.f, 14, rCol);
            ui.drawTextRight(activeHitCam.targetName, hcX + hcW - 10.f, hcY + 6.f, 12.5f, Color4::rgba(220, 230, 242));

            ui.drawText(activeHitCam.zoneName, hcX + 10.f, hcY + hcH - 36.f, 12.5f, Color4::rgba(242, 216, 128));
            std::snprintf(buf, sizeof(buf), "%s: пробитие %u мм  vs  привед. броня %u мм (%u°) • %u м",
                          activeHitCam.shellCode, activeHitCam.shellPen, activeHitCam.effectiveArmor,
                          activeHitCam.impactAngleDeg, activeHitCam.distanceM);
            ui.drawText(buf, hcX + 10.f, hcY + hcH - 18.f, 11.2f, Color4::rgba(196, 212, 232));
        }

        // Киллфид под окном камеры попаданий
        float kfY = (hitCamTimer > 0.f) ? 292.f : 62.f;
        for (const auto& kf : killFeed) {
            std::snprintf(buf, sizeof(buf), "%s  [%s%s]  %s",
                          kf.killer.c_str(), kf.weapon.c_str(), kf.ammoDet ? " • БК" : "", kf.victim.c_str());
            ui.drawTextRight(buf, W - 20.f, kfY, 12.5f,
                             kf.killerTeam == myTeam ? Color4::rgba(116, 202, 255) : Color4::rgba(252, 116, 106));
            kfY += 20.f;
        }

        // Центральный баннер уничтожения / крита
        if (bannerTimer > 0.f) {
            ui.drawTextCentered(bannerTitle.c_str(), W * 0.5f, H * 0.22f, 22.f, Color4::rgba(255, 214, 92));
            ui.drawTextCentered(bannerSub.c_str(),   W * 0.5f, H * 0.22f + 28.f, 15.f, Color4::rgba(238, 244, 252));
        }

        // 5. ЛЕВАЯ НИЖНЯЯ КУКЛА ПОВРЕЖДЕНИЙ ТАНКА И ТЕЛЕМЕТРИЯ (КАК В WAR THUNDER)
        if (hasWorldSnap && mySlot >= 0 && worldSnap.tanks[mySlot].active) {
            const NetTankSnap& me = worldSnap.tanks[mySlot];
            const VehicleSpec& mv = vehs[me.vehicleIdx];

            float dpX = 16.f, dpY = H - 178.f, dpW = 248.f, dpH = 162.f;
            ui.drawPanel(dpX, dpY, dpW, dpH, Color4::rgba(10, 15, 22, 0.86f), Color4::rgba(7, 10, 16, 0.92f), Color4::rgba(58, 76, 98, 0.7f));

            // Схема углов корпуса и башни
            float cx = dpX + 58.f, cy = dpY + 84.f;
            ui.drawCircleOutline(cx, cy, 42.f, 1.2f, Color4(1, 1, 1, 0.22f), 28);
            float relTur = myTurretYaw - myHullYaw;
            ui.fillRect(cx - 14.f, cy - 22.f, 28.f, 44.f, Color4::rgba(58, 128, 78, 0.75f));
            ui.drawRectOutline(cx - 14.f, cy - 22.f, 28.f, 44.f, 1.5f, Color4::rgba(132, 232, 156));
            ui.fillCircle(cx, cy, 12.f, Color4::rgba(82, 168, 106, 0.9f), 16);
            ui.drawLine(cx, cy, cx - std::sin(relTur) * 34.f, cy - std::cos(relTur) * 34.f, 3.0f, Color4::rgba(168, 248, 186));

            auto modColor = [](uint8_t h) {
                return h >= 80 ? Color4::rgba(116, 228, 142) : (h > 20 ? Color4::rgba(248, 196, 72) : Color4::rgba(248, 78, 68));
            };
            std::snprintf(buf, sizeof(buf), "СКОРОСТЬ: %.0f км/ч", std::fabs(mySpeed * 3.6f));
            ui.drawText(buf, dpX + 114.f, dpY + 12.f, 13.5f, Color4::rgba(240, 246, 255));
            std::snprintf(buf, sizeof(buf), "ЭКИПАЖ: %u / %d   ХП: %d", me.crew, mv.crew, me.hp);
            ui.drawText(buf, dpX + 114.f, dpY + 33.f, 12.f, Color4::rgba(206, 222, 240));

            ui.drawText("ДВИГАТЕЛЬ",   dpX + 114.f, dpY + 58.f,  11.5f, modColor(me.modHealth[0]));
            ui.drawText("КАЗЁННИК",    dpX + 114.f, dpY + 77.f,  11.5f, modColor(me.modHealth[1]));
            ui.drawText("ГУСЕНИЦЫ",    dpX + 114.f, dpY + 96.f,  11.5f, modColor(me.modHealth[2]));
            ui.drawText("ПРИВОД БАШНИ",dpX + 114.f, dpY + 115.f, 11.5f, modColor(me.modHealth[3]));

            if (!me.alive) {
                std::snprintf(buf, sizeof(buf), "ВОЗРОЖДЕНИЕ ЧЕРЕЗ %.1f С...", me.respawnTimer);
                ui.drawTextCentered(buf, W * 0.5f, H * 0.48f, 22.f, Color4::rgba(255, 112, 92));
            } else if (me.flags & 1) {
                ui.drawTextCentered("ПОЖАР В МТО! НАЖМИТЕ [6] ДЛЯ ТУШЕНИЯ ППО!", W * 0.5f, H * 0.74f, 17.f, Color4::rgba(255, 108, 56));
            } else if (me.flags & 2) {
                std::snprintf(buf, sizeof(buf), "ПОЛЕВОЙ РЕМОНТ МОДУЛЕЙ: %.1f С...", me.repairTimer);
                ui.drawTextCentered(buf, W * 0.5f, H * 0.74f, 16.f, Color4::rgba(252, 212, 96));
            }

            // 6. ЦЕНТРАЛЬНАЯ ПАНЕЛЬ ДЕЙСТВИЙ [1][2][3] СНАРЯДЫ, [G] ДЫМЫ, [F] РЕМОНТ, [6] ППО, [N] ТВП
            float barW = 472.f, barH = 62.f;
            float barX = W * 0.5f - barW * 0.5f, barY = H - barH - 12.f;
            ui.drawPanel(barX, barY, barW, barH, Color4::rgba(11, 15, 22, 0.88f), Color4::rgba(7, 10, 15, 0.94f), Color4::rgba(62, 78, 98, 0.75f));

            for (int s = 0; s < 3; ++s) {
                float sx = barX + 8.f + s * 74.f;
                bool sel = (selectedShellIdx == s);
                ui.drawPanel(sx, barY + 7.f, 68.f, 48.f,
                             sel ? Color4::rgba(46, 82, 122) : Color4::rgba(20, 26, 36),
                             Color4::rgba(14, 18, 26), sel ? Color4::rgba(248, 204, 88) : Color4::rgba(64, 78, 96));
                std::snprintf(buf, sizeof(buf), "[%d] %s", s + 1, mv.shells[s].code);
                ui.drawTextCentered(buf, sx + 34.f, barY + 12.f, 11.2f, Color4(1, 1, 1, 1));
                std::snprintf(buf, sizeof(buf), "%d мм", mv.shells[s].basePenMM);
                ui.drawTextCentered(buf, sx + 34.f, barY + 31.f, 11.5f, Color4::rgba(246, 206, 98));
            }

            auto utilSlot = [&](float ux, const char* key, const char* label, bool activeOrReady) {
                ui.drawPanel(ux, barY + 7.f, 58.f, 48.f, Color4::rgba(20, 28, 38), Color4::rgba(13, 18, 25),
                             activeOrReady ? Color4::rgba(98, 178, 228) : Color4::rgba(58, 66, 78));
                ui.drawTextCentered(key, ux + 29.f, barY + 11.f, 12.f, Color4::rgba(246, 212, 108));
                ui.drawTextCentered(label, ux + 29.f, barY + 31.f, 11.f, activeOrReady ? Color4::rgba(218, 232, 248) : Color4::rgba(130, 140, 155));
            };
            std::snprintf(buf, sizeof(buf), me.smokeCd > 0.1f ? "%.0fс" : "ДЫМЫ", me.smokeCd);
            utilSlot(barX + 232.f, "[G]", buf, me.smokeCd <= 0.1f);
            utilSlot(barX + 294.f, "[F]", "РЕМОНТ", (me.flags & 2) == 0);
            std::snprintf(buf, sizeof(buf), "ППО×%u", me.fpe);
            utilSlot(barX + 356.f, "[6]", buf, me.fpe > 0);
            utilSlot(barX + 418.f, "[N]", "ТВП", thermalMode);

            // Полоска перезарядки над панелью снарядов
            float rlFrac = (me.reloadTimer > 0.f) ? (1.f - me.reloadTimer / std::max(0.5f, mv.reloadSec)) : 1.f;
            ui.fillRect(barX, barY - 10.f, barW, 6.f, Color4(0, 0, 0, 0.65f));
            ui.fillRect(barX, barY - 10.f, barW * clampf(rlFrac, 0.f, 1.f), 6.f,
                        me.reloadTimer <= 0.01f ? Color4::rgba(98, 232, 132) : Color4::rgba(246, 172, 62));
        }

        // 7. ТАКТИЧЕСКАЯ МИНИ-КАРТА В ПРАВОМ НИЖНЕМ УГЛУ
        float mmSize = 172.f;
        float mmX = W - mmSize - 16.f, mmY = H - mmSize - 16.f;
        ui.drawPanel(mmX, mmY, mmSize, mmSize, Color4::rgba(12, 20, 24, 0.88f), Color4::rgba(8, 14, 18, 0.94f), Color4::rgba(76, 98, 118, 0.85f));
        for (int g = 1; g < 4; ++g) {
            ui.drawLine(mmX + g * (mmSize / 4.f), mmY, mmX + g * (mmSize / 4.f), mmY + mmSize, 1.f, Color4(1,1,1,0.08f));
            ui.drawLine(mmX, mmY + g * (mmSize / 4.f), mmX + mmSize, mmY + g * (mmSize / 4.f), 1.f, Color4(1,1,1,0.08f));
        }
        auto worldToMini = [&](float wx, float wz) -> Vec2 {
            float half = map.size * 0.5f;
            float nx = clampf((wx / half) * 0.5f + 0.5f, 0.03f, 0.97f);
            float nz = clampf((wz / half) * 0.5f + 0.5f, 0.03f, 0.97f);
            return Vec2(mmX + nx * mmSize, mmY + nz * mmSize);
        };
        for (size_t z = 0; z < map.zones.size() && z < 3; ++z) {
            Vec2 mp = worldToMini(map.zones[z].x, map.zones[z].z);
            int own = hasWorldSnap ? worldSnap.zoneOwner[z] : -1;
            Color4 zc = (own == myTeam) ? Color4::rgba(72, 168, 255) : (own >= 0 ? Color4::rgba(245, 78, 68) : Color4(1,1,1,0.85f));
            ui.drawCircleOutline(mp.x, mp.y, 7.f, 1.5f, zc, 16);
            char zCode[2] = {map.zones[z].id, 0};
            ui.drawTextCentered(zCode, mp.x, mp.y - 6.f, 10.5f, zc, false);
        }
        if (hasWorldSnap) {
            for (int i = 0; i < MAX_NET_TANKS; ++i) {
                const NetTankSnap& ts = worldSnap.tanks[i];
                if (!ts.active || !ts.alive) continue;
                Vec2 mp = worldToMini(renderTankPos[i].x, renderTankPos[i].z);
                if (i == mySlot) {
                    ui.fillCircle(mp.x, mp.y, 4.5f, Color4::rgba(116, 252, 146), 12);
                    ui.drawLine(mp.x, mp.y, mp.x - std::sin(camYaw) * 18.f, mp.y - std::cos(camYaw) * 18.f, 1.4f, Color4(1,1,1,0.55f));
                } else {
                    ui.fillCircle(mp.x, mp.y, 3.6f, ts.team == myTeam ? Color4::rgba(78, 174, 255) : Color4::rgba(248, 76, 66), 10);
                }
            }
        }

        // Чат боя слева над куклой танка
        float chY = H - 210.f - chatLines.size() * 19.f;
        for (const auto& cl : chatLines) {
            std::snprintf(buf, sizeof(buf), "[%s]: %s", cl.sender.c_str(), cl.text.c_str());
            ui.drawText(buf, 18.f, chY, 12.5f, cl.team == 2 ? Color4::rgba(248, 212, 108) : Color4::rgba(220, 234, 248));
            chY += 19.f;
        }
        if (chatInputActive) {
            ui.drawPanel(16.f, H - 208.f, 360.f, 26.f, Color4::rgba(14, 20, 30, 0.95f), Color4::rgba(10, 14, 22, 0.95f), Color4::rgba(116, 188, 255));
            std::snprintf(buf, sizeof(buf), "ЧАТ: %s_", chatDraft.c_str());
            ui.drawText(buf, 24.f, H - 203.f, 12.5f, Color4(1, 1, 1, 1));
        }

        // 8. ТАБЛИЦА СЧЁТА БОЯ (ПО ЗАЖАТИЮ TAB)
        if ((::GetAsyncKeyState(VK_TAB) & 0x8000) && hasWorldSnap) {
            float sbW = 720.f, sbH = 380.f;
            float sbX = W * 0.5f - sbW * 0.5f, sbY = H * 0.5f - sbH * 0.5f;
            ui.drawPanel(sbX, sbY, sbW, sbH, Color4::rgba(10, 15, 22, 0.95f), Color4::rgba(7, 10, 16, 0.97f), Color4::rgba(112, 142, 178));
            std::snprintf(buf, sizeof(buf), "СТАТИСТИКА ОНЛАЙН-БОЯ — %s", map.name);
            ui.drawTextCentered(buf, W * 0.5f, sbY + 12.f, 16.5f, Color4::rgba(244, 212, 108));

            int rowL = 0, rowR = 0;
            for (int i = 0; i < MAX_NET_TANKS; ++i) {
                const NetTankSnap& ts = worldSnap.tanks[i];
                if (!ts.active) continue;
                bool blue = (ts.team == 0);
                float rx = blue ? (sbX + 16.f) : (sbX + sbW * 0.5f + 8.f);
                float ry = sbY + 52.f + (blue ? rowL++ : rowR++) * 34.f;
                ui.fillRect(rx, ry, sbW * 0.5f - 24.f, 28.f,
                            i == mySlot ? Color4::rgba(48, 86, 62, 0.85f) : Color4::rgba(20, 28, 40, 0.8f));
                std::snprintf(buf, sizeof(buf), "%s (%s)", ts.callsign, vehs[ts.vehicleIdx].shortName);
                ui.drawText(buf, rx + 8.f, ry + 6.f, 12.f, blue ? Color4::rgba(136, 204, 255) : Color4::rgba(255, 142, 132));
                std::snprintf(buf, sizeof(buf), "%uФ / %uЗ / %uОЧК", ts.kills, ts.captures, ts.score);
                ui.drawTextRight(buf, rx + sbW * 0.5f - 32.f, ry + 6.f, 12.f, Color4(1, 1, 1, 1));
            }
        }

        // 9. МЕНЮ ПАУЗЫ ПО ESC
        if (pauseMenuOpen) {
            ui.fillRect(0, 0, W, H, Color4(0, 0, 0, 0.55f));
            float pw = 340.f, ph = 210.f, px = W * 0.5f - pw * 0.5f, py = H * 0.5f - ph * 0.5f;
            ui.drawPanel(px, py, pw, ph, Color4::rgba(14, 19, 28, 0.97f), Color4::rgba(9, 13, 20, 0.99f), Color4::rgba(218, 176, 78));
            ui.drawTextCentered("МЕНЮ КОМАНДИРА", W * 0.5f, py + 16.f, 18.f, Color4::rgba(246, 214, 118));

            ui.drawPanel(px + 28.f, py + 58.f, pw - 56.f, 40.f, Color4::rgba(38, 86, 128), Color4::rgba(24, 56, 86), Color4::rgba(116, 192, 255));
            ui.drawTextCentered("ПРОДОЛЖИТЬ БОЙ [ESC]", W * 0.5f, py + 68.f, 14.f, Color4(1, 1, 1, 1));
            addHit(ACT_PAUSE_RESUME, 0, px + 28.f, py + 58.f, pw - 56.f, 40.f);

            ui.drawPanel(px + 28.f, py + 110.f, pw - 56.f, 40.f, Color4::rgba(176, 42, 36), Color4::rgba(118, 24, 20), Color4::rgba(248, 124, 112));
            ui.drawTextCentered("ВЕРНУТЬСЯ В АНГАР", W * 0.5f, py + 120.f, 14.f, Color4(1, 1, 1, 1));
            addHit(ACT_PAUSE_RETURN_HANGAR, 0, px + 28.f, py + 110.f, pw - 56.f, 40.f);

            ui.drawTextCentered("F11 — Безрамочный / Оконный экран", W * 0.5f, py + 168.f, 11.5f, Color4::rgba(168, 184, 204));
        }
    }

    void onUiClick(float mx, float my) {
        // Идём с конца списка, чтобы модальные окна перехватывали клик первыми
        for (int i = (int)hitBoxes.size() - 1; i >= 0; --i) {
            const UiHitBox& hb = hitBoxes[i];
            if (!hb.contains(mx, my)) continue;
            sound.play(SND_UI_CLICK);

            switch (hb.id) {
                case ACT_SELECT_NATION:
                    profile.selectedNation = hb.param;
                    break;
                case ACT_SELECT_VEHICLE:
                    profile.selectedVehPerNation[profile.selectedNation] = hb.param;
                    showTechTree = false;
                    break;
                case ACT_SELECT_SHELL:
                    selectedShellIdx = hb.param;
                    break;
                case ACT_SELECT_CAMO:
                    profile.vehicleCamo[currentVehIdx()] = hb.param;
                    profile.save();
                    break;
                case ACT_SELECT_MAP:
                    selectedMapChoice = hb.param;
                    break;
                case ACT_TOGGLE_HANGAR_SCENE:
                    hangarBunkerMode = !hangarBunkerMode;
                    break;
                case ACT_SET_INSPECTOR:
                    inspectorMode = hb.param;
                    break;
                case ACT_TOGGLE_TECH_TREE:
                    showTechTree = !showTechTree;
                    break;
                case ACT_UNLOCK_VEHICLE: {
                    const VehicleSpec& uv = getVehicles()[hb.param];
                    if (!profile.isUnlocked(hb.param) && profile.rp >= uv.costRp && profile.silver >= uv.costSilver) {
                        profile.rp -= uv.costRp;
                        profile.silver -= uv.costSilver;
                        profile.unlockedMask |= (1u << hb.param);
                        profile.selectedVehPerNation[uv.nationIdx] = hb.param;
                        profile.save();
                    }
                    break;
                }
                case ACT_ENTER_BATTLE:
                    startBattle(hb.param);
                    break;
                case ACT_ENTER_POLIGON:
                    selectedMapChoice = 3;
                    startBattle(3);
                    break;
                case ACT_TOGGLE_SOUND:
                    sound.enabled = !sound.enabled;
                    settings.soundEnabled = sound.enabled;
                    settings.save();
                    break;
                case ACT_EXIT_GAME:
                    running = false;
                    ::PostQuitMessage(0);
                    break;
                case ACT_PAUSE_RESUME:
                    pauseMenuOpen = false;
                    break;
                case ACT_PAUSE_RETURN_HANGAR:
                    returnToHangar();
                    break;
                case ACT_OPEN_BLK_CDK: {
                    std::string blkDisk;
                    if (dagor::RuntimeKernel::instance().vfs.ensureEditableBlkOnDisk("gamedata/units/tanks_all_nations.blk", blkDisk)) {
                        for (char& c : blkDisk) if (c == '/') c = '\\';
                        ::ShellExecuteA(hwnd, "open", "notepad.exe", blkDisk.c_str(), nullptr, SW_SHOWNORMAL);
                    }
                    break;
                }
                case ACT_HOT_RELOAD_BLK:
                    hotReloadDagorBlk();
                    break;
            }
            return;
        }
    }

    void hotReloadDagorBlk() {
        initOrReloadDagorGamedata(true);
        const auto& vehs = getVehicles();
        for (size_t i = 0; i < vehs.size() && i < vehModels.size(); ++i) {
            buildVehicleMeshes(gfx.device, vehs[i], vehModels[i]);
        }
        buildEnvironmentAssets(gfx.device, envAssets);
        sound.play(SND_RELOAD_READY, 0.8f);
    }

    void toggleBorderless() {
        isBorderless = !isBorderless;
        if (isBorderless) {
            ::GetWindowRect(hwnd, &windowedRect);
            int sw = ::GetSystemMetrics(SM_CXSCREEN);
            int sh = ::GetSystemMetrics(SM_CYSCREEN);
            ::SetWindowLongPtrW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            ::SetWindowPos(hwnd, HWND_TOP, 0, 0, sw, sh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        } else {
            ::SetWindowLongPtrW(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            ::SetWindowPos(hwnd, HWND_NOTOPMOST, windowedRect.left, windowedRect.top,
                           windowedRect.right - windowedRect.left, windowedRect.bottom - windowedRect.top,
                           SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }
    }

    void onKeyDown(WPARAM key) {
        if (key == VK_F11) {
            toggleBorderless();
            return;
        }
        if (appState == 0) {
            if (key == VK_F5) hotReloadDagorBlk();
            else if (key == 'T') showTechTree = !showTechTree;
            else if (key == VK_ESCAPE && showTechTree) showTechTree = false;
            else if (key >= '1' && key <= '3') selectedShellIdx = int(key - '1');
            return;
        }

        if (chatInputActive) {
            if (key == VK_RETURN) {
                if (!chatDraft.empty()) {
                    PktChat pc;
                    pc.team = (uint8_t)myTeam;
                    std::snprintf(pc.sender, sizeof(pc.sender), "%s", settings.callsign);
                    std::snprintf(pc.text, sizeof(pc.text), "%s", chatDraft.c_str());
                    sendPacket(&pc, sizeof(pc));
                }
                chatDraft.clear();
                chatInputActive = false;
            } else if (key == VK_ESCAPE) {
                chatDraft.clear();
                chatInputActive = false;
            } else if (key == VK_BACK && !chatDraft.empty()) {
                chatDraft.pop_back();
            }
            return;
        }

        if (key == VK_ESCAPE) {
            pauseMenuOpen = !pauseMenuOpen;
        } else if (key == VK_RETURN) {
            chatInputActive = true;
            chatDraft.clear();
        } else if (key == VK_SHIFT) {
            sniperMode = !sniperMode;
        } else if (key == 'N') {
            thermalMode = !thermalMode;
        } else if (key == 'R') {
            measuredRangeM = int((actualBoreWorld - Vec3(myX, myY, myZ)).length() * 1.4f);
        } else if (key >= '1' && key <= '3') {
            selectedShellIdx = int(key - '1');
        } else if (key == 'G' && mySlot >= 0) {
            PktAction a; a.slot = (uint8_t)mySlot; a.action = 1;
            sendPacket(&a, sizeof(a));
        } else if (key == 'F' && mySlot >= 0) {
            PktAction a; a.slot = (uint8_t)mySlot; a.action = 2;
            sendPacket(&a, sizeof(a));
        } else if (key == '6' && mySlot >= 0) {
            PktAction a; a.slot = (uint8_t)mySlot; a.action = 3;
            sendPacket(&a, sizeof(a));
        }
    }

    void shutdown() {
        profile.save();
        settings.save();
        captureMouse(false);
        localServer.stop();
        if (clientSock != INVALID_SOCKET) { ::closesocket(clientSock); clientSock = INVALID_SOCKET; }
        for (auto& m : vehModels) m.release();
        envAssets.release();
        ui.shutdown();
        gfx.shutdown();
        sound.shutdown();
        steam.shutdown();
        ::WSACleanup();
    }
};

static SteelFrontApp gApp;

static LRESULT CALLBACK GameWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED && gApp.gfx.swapChain) {
                gApp.gfx.onResize(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        case WM_MOUSEMOVE:
            gApp.mouseX = (float)GET_X_LPARAM(lParam);
            gApp.mouseY = (float)GET_Y_LPARAM(lParam);
            if (gApp.appState == 0 && gApp.hangarDragging) {
                int dx = GET_X_LPARAM(lParam) - gApp.lastMousePt.x;
                int dy = GET_Y_LPARAM(lParam) - gApp.lastMousePt.y;
                gApp.hangarYaw   = wrapAngle(gApp.hangarYaw + dx * 0.0065f);
                gApp.hangarPitch = clampf(gApp.hangarPitch + dy * 0.005f, 0.05f, 0.72f);
                gApp.lastMousePt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            }
            return 0;
        case WM_LBUTTONDOWN:
            gApp.mouseX = (float)GET_X_LPARAM(lParam);
            gApp.mouseY = (float)GET_Y_LPARAM(lParam);
            if (gApp.appState == 0 || gApp.pauseMenuOpen) {
                size_t beforeState = gApp.appState;
                // Проверяем попадание по кнопке интерфейса; если мимо — вращаем камеру ангара
                bool hitAny = false;
                for (const auto& hb : gApp.hitBoxes) if (hb.contains(gApp.mouseX, gApp.mouseY)) { hitAny = true; break; }
                if (hitAny) {
                    gApp.onUiClick(gApp.mouseX, gApp.mouseY);
                } else if (beforeState == 0 && !gApp.showTechTree) {
                    gApp.hangarDragging = true;
                    gApp.lastMousePt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                    ::SetCapture(hWnd);
                }
            }
            return 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            if (gApp.hangarDragging) {
                gApp.hangarDragging = false;
                ::ReleaseCapture();
            }
            return 0;
        case WM_RBUTTONDOWN:
            if (gApp.appState == 0 && !gApp.showTechTree) {
                gApp.hangarDragging = true;
                gApp.lastMousePt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ::SetCapture(hWnd);
            } else if (gApp.appState == 1 && !gApp.pauseMenuOpen) {
                gApp.sniperMode = !gApp.sniperMode;
            }
            return 0;
        case WM_MOUSEWHEEL: {
            float steps = float(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            if (gApp.appState == 0) {
                gApp.hangarDist = clampf(gApp.hangarDist - steps * 0.65f, 5.2f, 14.5f);
            } else if (gApp.sniperMode) {
                gApp.sniperZoom = clampf(gApp.sniperZoom + steps * 1.5f, 3.5f, 12.0f);
            }
            return 0;
        }
        case WM_KEYDOWN:
            gApp.onKeyDown(wParam);
            return 0;
        case WM_CHAR:
            if (gApp.chatInputActive && wParam >= 32 && gApp.chatDraft.size() < 70) {
                wchar_t wc = (wchar_t)wParam;
                char utf8[8] = {};
                int n = ::WideCharToMultiByte(CP_UTF8, 0, &wc, 1, utf8, sizeof(utf8), nullptr, nullptr);
                if (n > 0) gApp.chatDraft.append(utf8, n);
            }
            return 0;
        case WM_DESTROY:
            gApp.running = false;
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    ::SetProcessDPIAware();
    ensureGameRootWorkingDir();

    GameSettings initCfg;
    initCfg.load();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = GameWndProc;
    wc.hInstance = hInst;
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SteelFrontDX11Wnd";
    ::RegisterClassExW(&wc);

    int scrW = ::GetSystemMetrics(SM_CXSCREEN);
    int scrH = ::GetSystemMetrics(SM_CYSCREEN);
    int winW = scrW, winH = scrH, winX = 0, winY = 0;
    DWORD style = WS_POPUP | WS_VISIBLE;
    gApp.isBorderless = (initCfg.displayMode == 0);

    if (!gApp.isBorderless) {
        style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        winW = (initCfg.displayMode == 2) ? 1280 : 1600;
        winH = (initCfg.displayMode == 2) ? 720  : 900;
        winX = std::max(0, (scrW - winW) / 2);
        winY = std::max(0, (scrH - winH) / 2);
    }

    HWND hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"Steel Front: Online — Dagor Engine 6.5 (VROMFS + .BLK + daSkies)",
                                  style, winX, winY, winW, winH, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;

    BOOL dark = TRUE;
    ::DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));

    RECT cr{};
    ::GetClientRect(hwnd, &cr);
    if (!gApp.init(hwnd, cr.right - cr.left, cr.bottom - cr.top)) {
        return 1;
    }

    LARGE_INTEGER freq{}, prev{};
    ::QueryPerformanceFrequency(&freq);
    ::QueryPerformanceCounter(&prev);

    MSG msg{};
    while (gApp.running) {
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) gApp.running = false;
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
        if (!gApp.running) break;

        LARGE_INTEGER now{};
        ::QueryPerformanceCounter(&now);
        float dt = float(double(now.QuadPart - prev.QuadPart) / double(freq.QuadPart));
        prev = now;
        dt = clampf(dt, 0.0005f, 0.05f);

        gApp.update(dt);
        gApp.renderScene3D();
        gApp.renderUi2D();
        gApp.gfx.present();
    }

    gApp.shutdown();
    return 0;
}
