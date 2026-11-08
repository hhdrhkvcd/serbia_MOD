#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include "game_data.h"
#include "net_protocol.h"
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>

namespace sf {

struct SimTank {
    bool     active = false;
    bool     isBot = false;
    bool     isDummy = false;
    bool     alive = true;
    int      team = 0; // 0=blue, 1=red
    int      vehicleIdx = 1;
    std::string callsign;
    sockaddr_in addr{};
    bool     hasClient = false;
    double   lastSeenSec = 0.0;

    float    x = 0, y = 0, z = 0;
    float    hullYaw = 0, turretYaw = 0, gunPitch = 0, speed = 0;
    int      hp = 2400, maxHp = 2400;
    int      crew = 3, maxCrew = 3;
    float    reloadTimer = 0, smokeCd = 0, respawnTimer = 0, spawnProtect = 0;
    bool     onFire = false;
    float    fireTimer = 0;
    int      fireAttackerSlot = -1;
    bool     repairing = false;
    float    repairTimer = 0;
    int      fpeCount = 2;
    float    modEngine = 1.f, modBreech = 1.f, modTracks = 1.f, modRing = 1.f;
    int      kills = 0, deaths = 0, captures = 0, score = 0;

    // Параметры ИИ / полигона
    int      aiTargetZone = 1;
    float    aiRethink = 1.f;
    float    aiStrafe = 1.f;
    float    dummyHomeX = 0, dummyHomeZ = 0, dummyHomeYaw = 0, dummyPhase = 0;
    bool     dummyMoving = false;
};

struct SimProjectile {
    int       shooterSlot = -1;
    int       team = 0;
    ShellSpec shell{};
    Vec3      pos;
    Vec3      vel;
    float     simSpeed = 300.f;
    bool      guided = false;
    bool      hasAim = false;
    Vec3      aimTarget;
    float     distMeters = 0.f;
    float     ttl = 3.2f;
};

struct SimSmoke {
    Vec3  pos;
    float radius = 12.f;
    float ttl = 15.f;
};

class SimServer {
public:
    SOCKET sock = INVALID_SOCKET;
    uint16_t port = DEFAULT_SERVER_PORT;
    int mapIdx = 0;
    float tickets[2] = {2500.f, 2500.f};
    float timeLeftSec = 900.f;
    int   zoneOwner[3] = {-1, -1, -1};
    float zoneProg[3]  = {0.f, 0.f, 0.f};
    double uptimeSec = 0.0;
    float snapshotTimer = 0.f;

    SimTank tanks[MAX_NET_TANKS];
    std::vector<SimProjectile> projectiles;
    std::vector<SimSmoke> smokes;

    bool start(uint16_t listenPort, int initialMapIdx = 0) {
        stop();
        port = listenPort;
        sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) return false;

        u_long nonBlock = 1;
        ::ioctlsocket(sock, FIONBIO, &nonBlock);

        int sndBuf = 262144, rcvBuf = 262144;
        ::setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&sndBuf, sizeof(sndBuf));
        ::setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvBuf, sizeof(rcvBuf));

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = INADDR_ANY;
        local.sin_port = htons(port);
        if (::bind(sock, (sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
            ::closesocket(sock);
            sock = INVALID_SOCKET;
            return false;
        }

        setupMap(initialMapIdx);
        return true;
    }

    void stop() {
        if (sock != INVALID_SOCKET) {
            ::closesocket(sock);
            sock = INVALID_SOCKET;
        }
    }

    bool isRunning() const { return sock != INVALID_SOCKET; }

    void setupMap(int newMapIdx) {
        const auto& maps = getMaps();
        mapIdx = (newMapIdx >= 0 && newMapIdx < (int)maps.size()) ? newMapIdx : 0;
        tickets[0] = tickets[1] = 2500.f;
        timeLeftSec = 900.f;
        for (int i = 0; i < 3; ++i) { zoneOwner[i] = -1; zoneProg[i] = 0.f; }
        projectiles.clear();
        smokes.clear();

        // Сохраняем подключённых живых клиентов, пересоздаём ботов/мишени
        for (int i = 0; i < MAX_NET_TANKS; ++i) {
            if (!tanks[i].hasClient) {
                tanks[i] = SimTank{};
            } else {
                tanks[i].kills = tanks[i].deaths = tanks[i].captures = tanks[i].score = 0;
                if (maps[mapIdx].isPoligon) tanks[i].team = 0;
                spawnTank(i);
            }
        }

        if (maps[mapIdx].isPoligon) {
            setupPoligonDummies();
        } else {
            rebalanceBots();
        }
    }

    void setupPoligonDummies() {
        struct DummyCfg { int veh; const char* name; float x, z, yaw; bool mov; };
        const DummyCfg cfg[6] = {
            {4,  "Мишень 120м • M-80AB1",   -18.f,  35.f, PI - 0.25f, false},
            {0,  "Мишень 190м • Т-72Б3",      0.f, -15.f, PI,         false},
            {7,  "Мишень 250м • M-84AS2",    24.f, -55.f, PI + 0.52f, false},
            {13, "Мишень 320м • M1A2 SEP3", -24.f,-100.f, PI * 0.55f, false},
            {16, "Мишень 390м • Leo 2A7V",   12.f,-145.f, PI - 0.12f, false},
            {10, "Движ. цель • ZTZ-99A",    -28.f, -78.f, PI * 0.5f,  true}
        };
        int placed = 0;
        for (int i = 0; i < MAX_NET_TANKS && placed < 6; ++i) {
            if (tanks[i].active) continue;
            const auto& d = cfg[placed++];
            const auto& v = getVehicles()[d.veh];
            SimTank& t = tanks[i];
            t = SimTank{};
            t.active = true;
            t.isDummy = true;
            t.alive = true;
            t.team = 1;
            t.vehicleIdx = d.veh;
            t.callsign = d.name;
            t.x = t.dummyHomeX = d.x;
            t.z = t.dummyHomeZ = d.z;
            t.y = getTerrainHeight(mapIdx, t.x, t.z);
            t.hullYaw = t.turretYaw = t.dummyHomeYaw = d.yaw;
            t.dummyMoving = d.mov;
            t.hp = t.maxHp = v.hp;
            t.crew = t.maxCrew = v.crew;
        }
    }

    void rebalanceBots() {
        const auto& maps = getMaps();
        if (maps[mapIdx].isPoligon) return;

        static const char* blueNames[6] = {"[БОТ] Волк_80У", "[БОТ] Војвода_СРБ", "[БОТ] Прорыв_90", "[БОТ] Вихор_М91", "[БОТ] Скат_БВМ", "[БОТ] Дракон_99A"};
        static const char* redNames[6]  = {"[БОТ] Panzer_2A7", "[БОТ] Abrams_SEP3", "[БОТ] Strv_122B", "[БОТ] Leclerc_XLR", "[БОТ] Challenger2", "[БОТ] Ghost_CV90"};
        static const int blueVehs[6] = {1, 7, 3, 6, 2, 10};
        static const int redVehs[6]  = {16, 13, 22, 19, 15, 20};

        for (int team = 0; team < 2; ++team) {
            int count = 0;
            for (int i = 0; i < MAX_NET_TANKS; ++i) {
                if (tanks[i].active && tanks[i].team == team) count++;
            }
            const int targetPerTeam = 6;
            while (count < targetPerTeam) {
                int slot = -1;
                for (int i = 0; i < MAX_NET_TANKS; ++i) if (!tanks[i].active) { slot = i; break; }
                if (slot < 0) break;
                SimTank& b = tanks[slot];
                b = SimTank{};
                b.active = true;
                b.isBot = true;
                b.team = team;
                b.vehicleIdx = (team == 0) ? blueVehs[count % 6] : redVehs[count % 6];
                b.callsign = (team == 0) ? blueNames[count % 6] : redNames[count % 6];
                b.aiTargetZone = count % 3;
                b.aiStrafe = (slot % 2 == 0) ? 1.f : -1.f;
                spawnTank(slot);
                count++;
            }
            while (count > targetPerTeam) {
                int botSlot = -1;
                for (int i = MAX_NET_TANKS - 1; i >= 0; --i) {
                    if (tanks[i].active && tanks[i].isBot && tanks[i].team == team) { botSlot = i; break; }
                }
                if (botSlot < 0) break;
                tanks[botSlot] = SimTank{};
                count--;
            }
        }
    }

    void spawnTank(int slot) {
        SimTank& t = tanks[slot];
        const auto& map = getMaps()[mapIdx];
        const auto& v = getVehicles()[t.vehicleIdx];
        const auto& spList = (t.team == 0 || map.redSpawns.empty()) ? map.blueSpawns : map.redSpawns;
        const SpawnPoint& sp = spList[slot % spList.size()];

        float jx = ((slot * 37) % 13 - 6) * 1.4f;
        float jz = ((slot * 53) % 13 - 6) * 1.4f;
        t.x = sp.x + jx;
        t.z = sp.z + jz;
        resolveMapCollision(map, t.x, t.z, 2.6f);
        t.y = getTerrainHeight(mapIdx, t.x, t.z);
        t.hullYaw = t.turretYaw = sp.yaw;
        t.gunPitch = 0.f;
        t.speed = 0.f;
        t.alive = true;
        t.hp = t.maxHp = v.hp;
        t.crew = t.maxCrew = v.crew;
        t.reloadTimer = 0.f;
        t.smokeCd = 0.f;
        t.onFire = false;
        t.repairing = false;
        t.fpeCount = 2;
        t.spawnProtect = t.isDummy ? 0.f : 3.5f;
        t.modEngine = t.modBreech = t.modTracks = t.modRing = 1.f;
    }

    void sendToAddr(const sockaddr_in& to, const void* data, int len) {
        if (sock != INVALID_SOCKET) {
            ::sendto(sock, (const char*)data, len, 0, (const sockaddr*)&to, sizeof(to));
        }
    }

    void broadcastToClients(const void* data, int len) {
        for (int i = 0; i < MAX_NET_TANKS; ++i) {
            if (tanks[i].active && tanks[i].hasClient) {
                sendToAddr(tanks[i].addr, data, len);
            }
        }
    }

    static bool sameEndpoint(const sockaddr_in& a, const sockaddr_in& b) {
        return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
    }

    int countHumans() const {
        int c = 0;
        for (int i = 0; i < MAX_NET_TANKS; ++i) if (tanks[i].active && tanks[i].hasClient) c++;
        return std::max(1, c);
    }

    void pollNetwork() {
        if (sock == INVALID_SOCKET) return;
        uint8_t buf[1500];
        sockaddr_in from{};
        int fromLen = sizeof(from);
        while (true) {
            fromLen = sizeof(from);
            int r = ::recvfrom(sock, (char*)buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
            if (r <= (int)sizeof(PktHeader)) break;
            const auto* hdr = reinterpret_cast<const PktHeader*>(buf);
            if (hdr->magic != NET_MAGIC) continue;

            if (hdr->type == PKT_PING && r >= (int)sizeof(PktPing)) {
                const auto* p = reinterpret_cast<const PktPing*>(buf);
                PktPong pong;
                pong.clientTimeMs = p->clientTimeMs;
                pong.onlineHumans = (uint8_t)countHumans();
                pong.mapIdx = (uint8_t)mapIdx;
                sendToAddr(from, &pong, sizeof(pong));
            }
            else if (hdr->type == PKT_JOIN && r >= (int)sizeof(PktJoin)) {
                const auto* j = reinterpret_cast<const PktJoin*>(buf);
                int reqMap = j->preferredMapIdx < getMaps().size() ? j->preferredMapIdx : 0;
                // Если игроков ещё не было или запрошен Полигон/другая карта единственным игроком — переключаем карту
                int existingHumans = 0;
                int existingSlot = -1;
                for (int i = 0; i < MAX_NET_TANKS; ++i) {
                    if (tanks[i].active && tanks[i].hasClient) {
                        if (sameEndpoint(tanks[i].addr, from)) existingSlot = i;
                        else existingHumans++;
                    }
                }
                if (existingHumans == 0 && reqMap != mapIdx) {
                    setupMap(reqMap);
                }

                int slot = existingSlot;
                if (slot < 0) {
                    // Освобождаем слот у бота, если все 16 слотов заняты
                    for (int i = 0; i < MAX_NET_TANKS; ++i) {
                        if (!tanks[i].active) { slot = i; break; }
                    }
                    if (slot < 0) {
                        for (int i = MAX_NET_TANKS - 1; i >= 0; --i) {
                            if (tanks[i].isBot) { tanks[i] = SimTank{}; slot = i; break; }
                        }
                    }
                }
                if (slot >= 0) {
                    int blueH = 0, redH = 0;
                    for (int i = 0; i < MAX_NET_TANKS; ++i) {
                        if (i != slot && tanks[i].active && tanks[i].hasClient) {
                            if (tanks[i].team == 0) blueH++; else redH++;
                        }
                    }
                    SimTank& t = tanks[slot];
                    t = SimTank{};
                    t.active = true;
                    t.hasClient = true;
                    t.addr = from;
                    t.lastSeenSec = uptimeSec;
                    t.team = getMaps()[mapIdx].isPoligon ? 0 : (blueH <= redH ? 0 : 1);
                    t.vehicleIdx = j->vehicleIdx < getVehicles().size() ? j->vehicleIdx : 1;
                    char safeName[25] = {};
                    std::memcpy(safeName, j->callsign, 24);
                    t.callsign = safeName[0] ? safeName : "Танкист";
                    spawnTank(slot);
                    if (!getMaps()[mapIdx].isPoligon) rebalanceBots();

                    PktJoinAck ack;
                    ack.assignedSlot = (uint8_t)slot;
                    ack.team = (uint8_t)t.team;
                    ack.mapIdx = (uint8_t)mapIdx;
                    sendToAddr(from, &ack, sizeof(ack));

                    if (!getMaps()[mapIdx].isPoligon) {
                        PktChat sysChat;
                        sysChat.team = 2;
                        std::snprintf(sysChat.sender, sizeof(sysChat.sender), "ШТАБ");
                        std::snprintf(sysChat.text, sizeof(sysChat.text), "%s (%s) вступил в онлайн-бой!",
                                      t.callsign.c_str(), getVehicles()[t.vehicleIdx].shortName);
                        broadcastToClients(&sysChat, sizeof(sysChat));
                    }
                }
            }
            else if (hdr->type == PKT_CLIENT_STATE && r >= (int)sizeof(PktClientState)) {
                const auto* st = reinterpret_cast<const PktClientState*>(buf);
                if (st->slot < MAX_NET_TANKS && tanks[st->slot].hasClient && sameEndpoint(tanks[st->slot].addr, from)) {
                    SimTank& t = tanks[st->slot];
                    t.lastSeenSec = uptimeSec;
                    if (t.alive) {
                        if (!t.repairing && t.modEngine > 0.f && t.modTracks > 0.f) {
                            float nx = st->x, nz = st->z;
                            resolveMapCollision(getMaps()[mapIdx], nx, nz, 2.4f);
                            t.x = nx; t.z = nz;
                            t.y = getTerrainHeight(mapIdx, t.x, t.z);
                            t.hullYaw = st->hullYaw;
                            t.speed = st->speed;
                        } else {
                            t.speed = 0.f;
                        }
                        t.turretYaw = st->turretYaw;
                        t.gunPitch = clampf(st->gunPitch, -0.18f, 0.42f);
                    }
                }
            }
            else if (hdr->type == PKT_FIRE && r >= (int)sizeof(PktFire)) {
                const auto* f = reinterpret_cast<const PktFire*>(buf);
                if (f->slot < MAX_NET_TANKS && tanks[f->slot].hasClient && sameEndpoint(tanks[f->slot].addr, from)) {
                    tanks[f->slot].lastSeenSec = uptimeSec;
                    Vec3 orig(f->ox, f->oy, f->oz);
                    Vec3 dir = Vec3(f->dx, f->dy, f->dz).normalized();
                    fireCannon(f->slot, f->shellIdx, &orig, &dir);
                }
            }
            else if (hdr->type == PKT_GUIDE_ATGM && r >= (int)sizeof(PktGuideAtgm)) {
                const auto* g = reinterpret_cast<const PktGuideAtgm*>(buf);
                for (auto& pr : projectiles) {
                    if (pr.shooterSlot == g->slot && pr.guided) {
                        pr.hasAim = true;
                        pr.aimTarget = Vec3(g->tx, g->ty, g->tz);
                    }
                }
            }
            else if (hdr->type == PKT_ACTION && r >= (int)sizeof(PktAction)) {
                const auto* a = reinterpret_cast<const PktAction*>(buf);
                if (a->slot < MAX_NET_TANKS && tanks[a->slot].hasClient && sameEndpoint(tanks[a->slot].addr, from)) {
                    SimTank& t = tanks[a->slot];
                    t.lastSeenSec = uptimeSec;
                    if (a->action == 1) deploySmoke(a->slot);
                    else if (a->action == 2 && t.alive && !t.repairing) {
                        if (t.modEngine < 1.f || t.modBreech < 1.f || t.modTracks < 1.f || t.modRing < 1.f) {
                            t.repairing = true;
                            t.repairTimer = 5.5f;
                        }
                    }
                    else if (a->action == 3 && t.alive && t.onFire && t.fpeCount > 0) {
                        t.onFire = false;
                        t.fpeCount--;
                    }
                    else if (a->action == 4 && t.alive) {
                        PktFxEvent fx;
                        fx.fxKind = FX_MG_TRACER;
                        fx.shooterOrTargetSlot = a->slot;
                        fx.x = a->ox; fx.y = a->oy; fx.z = a->oz;
                        fx.dx = a->dx; fx.dy = a->dy; fx.dz = a->dz;
                        fx.speed = 220.f;
                        broadcastToClients(&fx, sizeof(fx));
                    }
                    else if (a->action == 5) {
                        tanks[a->slot] = SimTank{};
                        rebalanceBots();
                    }
                }
            }
            else if (hdr->type == PKT_CHAT && r >= (int)sizeof(PktChat)) {
                const auto* c = reinterpret_cast<const PktChat*>(buf);
                PktChat out = *c;
                out.hdr = {NET_MAGIC, PKT_CHAT};
                broadcastToClients(&out, sizeof(out));
            }
        }
    }

    void fireCannon(int slot, int shellIdx, const Vec3* customOrig = nullptr, const Vec3* customDir = nullptr) {
        SimTank& t = tanks[slot];
        if (!t.alive || t.reloadTimer > 0.f || t.modBreech <= 0.2f || t.repairing) return;
        const auto& v = getVehicles()[t.vehicleIdx];
        int sIdx = (shellIdx >= 0 && shellIdx < 3) ? shellIdx : 0;
        const ShellSpec& sh = v.shells[sIdx];
        t.reloadTimer = v.reloadSec;

        Vec3 dir = customDir ? *customDir : Vec3(
            -std::sin(t.turretYaw) * std::cos(t.gunPitch),
             std::sin(t.gunPitch),
            -std::cos(t.turretYaw) * std::cos(t.gunPitch)
        ).normalized();
        Vec3 orig = customOrig ? *customOrig : (Vec3(t.x, t.y + 1.72f, t.z) + dir * 3.8f);

        float simSpd = sh.guided ? 88.f : std::max(190.f, sh.speedMs * 0.24f);
        SimProjectile pr;
        pr.shooterSlot = slot;
        pr.team = t.team;
        pr.shell = sh;
        pr.pos = orig;
        pr.vel = dir * simSpd;
        pr.simSpeed = simSpd;
        pr.guided = sh.guided;
        pr.ttl = sh.guided ? 6.5f : 3.2f;
        projectiles.push_back(pr);

        PktFxEvent fx;
        fx.fxKind = FX_CANNON_FIRE;
        fx.shooterOrTargetSlot = (uint8_t)slot;
        fx.shellType = (uint8_t)sh.type;
        fx.x = orig.x; fx.y = orig.y; fx.z = orig.z;
        fx.dx = dir.x; fx.dy = dir.y; fx.dz = dir.z;
        fx.speed = sh.guided ? 0.f : simSpd;
        broadcastToClients(&fx, sizeof(fx));
    }

    void deploySmoke(int slot) {
        SimTank& t = tanks[slot];
        if (!t.alive || t.smokeCd > 0.f) return;
        t.smokeCd = 20.f;
        float sx = t.x - std::sin(t.turretYaw) * 11.f;
        float sz = t.z - std::cos(t.turretYaw) * 11.f;
        float sy = getTerrainHeight(mapIdx, sx, sz);
        smokes.push_back({Vec3(sx, sy, sz), 11.5f, 15.f});

        PktFxEvent fx;
        fx.fxKind = FX_SMOKE_SCREEN;
        fx.x = sx; fx.y = sy; fx.z = sz;
        broadcastToClients(&fx, sizeof(fx));
    }

    bool lineConcealedBySmoke(float x1, float z1, float x2, float z2) const {
        float dx = x2 - x1, dz = z2 - z1;
        float lenSq = dx * dx + dz * dz + 1e-4f;
        for (const auto& s : smokes) {
            float u = clampf(((s.pos.x - x1) * dx + (s.pos.z - z1) * dz) / lenSq, 0.f, 1.f);
            if (std::hypot(x1 + dx * u - s.pos.x, z1 + dz * u - s.pos.z) < s.radius) return true;
        }
        return false;
    }

    void evaluateHit(const SimProjectile& pr, int targetSlot) {
        SimTank& target = tanks[targetSlot];
        const auto& veh = getVehicles()[target.vehicleIdx];

        float relX = pr.pos.x - target.x;
        float relY = pr.pos.y - target.y;
        float relZ = pr.pos.z - target.z;
        bool isTurret = relY > 1.25f;
        float refYaw = isTurret ? target.turretYaw : target.hullYaw;
        float c = std::cos(-refYaw), s = std::sin(-refYaw);

        float localX = relX * c - relZ * s;
        float localZ = relX * s + relZ * c;
        Vec3 nDir = pr.vel.normalized();
        Vec3 localDir(nDir.x * c - nDir.z * s, nDir.y, nDir.x * s + nDir.z * c);

        const char* zoneName = "Верхняя лобовая деталь (ВЛД)";
        int nominalMM = veh.hullArmor[0];
        Vec3 plateNormal(0.f, 0.34f, -0.94f);
        float eraFactor = 1.f;

        if (isTurret) {
            if (localZ < -0.25f && std::fabs(localX) < 1.15f) {
                if (std::fabs(localX) < 0.34f) {
                    zoneName = "Маска орудия";
                    nominalMM = int(veh.turretArmor[0] * 0.56f);
                    plateNormal = Vec3(0.f, 0.08f, -0.99f).normalized();
                    eraFactor = 0.2f;
                } else {
                    zoneName = "Скула башни";
                    nominalMM = veh.turretArmor[0];
                    plateNormal = Vec3((localX >= 0 ? 0.28f : -0.28f), 0.22f, -0.93f).normalized();
                    eraFactor = 1.f;
                }
            } else if (localZ > 0.55f && std::fabs(localX) < 1.1f) {
                zoneName = "Корма башни";
                nominalMM = veh.turretArmor[2];
                plateNormal = Vec3(0.f, 0.05f, 1.f).normalized();
                eraFactor = 0.f;
            } else {
                zoneName = "Борт башни";
                nominalMM = veh.turretArmor[1];
                plateNormal = Vec3(localX >= 0 ? 1.f : -1.f, 0.1f, 0.f).normalized();
                eraFactor = 0.35f;
            }
        } else {
            if (localZ < -0.85f && std::fabs(localX) < 1.38f) {
                if (relY < 0.68f) {
                    zoneName = "Нижняя лобовая деталь (НЛД)";
                    nominalMM = int(veh.hullArmor[0] * 0.48f);
                    plateNormal = Vec3(0.f, -0.22f, -0.97f).normalized();
                    eraFactor = 0.15f;
                } else {
                    zoneName = "Верхняя лобовая деталь (ВЛД)";
                    nominalMM = veh.hullArmor[0];
                    plateNormal = Vec3(0.f, 0.34f, -0.94f).normalized();
                    eraFactor = 1.f;
                }
            } else if (localZ > 0.95f && std::fabs(localX) < 1.38f) {
                zoneName = "Корма корпуса (МТО)";
                nominalMM = veh.hullArmor[2];
                plateNormal = Vec3(0.f, 0.08f, 0.99f).normalized();
                eraFactor = 0.f;
            } else {
                zoneName = relY < 0.62f ? "Ходовая часть / Гусеница" : "Борт корпуса";
                nominalMM = veh.hullArmor[1];
                plateNormal = Vec3(localX >= 0 ? 1.f : -1.f, 0.05f, 0.f).normalized();
                eraFactor = 0.45f;
            }
        }

        float cosImp = std::fabs(Vec3::dot(localDir, plateNormal));
        int impactDeg = int(rad2deg(std::acos(clampf(cosImp, 0.f, 1.f))));

        int shellPen = pr.shell.basePenMM;
        if (pr.shell.type == SHELL_APFSDS) {
            shellPen = int(pr.shell.basePenMM * std::max(0.78f, 1.f - (pr.distMeters / 2500.f) * 0.16f));
        }
        float slopeMult = (pr.shell.type == SHELL_APFSDS)
            ? (1.f / std::max(0.38f, std::pow(cosImp, 0.78f)))
            : (1.f / std::max(0.32f, cosImp));

        float eraBonus = 0.f;
        if (pr.shell.type == SHELL_HEATFS) eraBonus = 190.f * eraFactor;
        else if (pr.shell.type == SHELL_APFSDS) eraBonus = 35.f * eraFactor;
        else if (pr.shell.type == SHELL_ATGM) eraBonus = 45.f * eraFactor;

        int effArmor = int(nominalMM * slopeMult + eraBonus);
        int ricochetDeg = (pr.shell.type == SHELL_APFSDS) ? 75 : 79;

        uint8_t resKind = HIT_NONPEN;
        uint8_t modBits = 0;
        bool ammoDet = false;
        int hpLoss = 0;

        if (target.spawnProtect > 0.f && !target.isDummy) {
            resKind = HIT_SPAWN_PROTECT;
        } else if (pr.shell.type != SHELL_HE && impactDeg >= ricochetDeg) {
            resKind = HIT_RICOCHET;
        } else if (pr.shell.type == SHELL_HE) {
            if (shellPen >= nominalMM * 0.65f || nominalMM <= 95) {
                resKind = HIT_PENETRATION;
                hpLoss = int(pr.shell.damage * 1.1f);
            } else {
                resKind = HIT_SPALL;
                hpLoss = int(pr.shell.damage * 0.22f);
                if (std::rand() % 100 < 45) { target.modTracks = 0.f; modBits |= MODBIT_TRACKS; }
            }
        } else if (shellPen >= effArmor) {
            resKind = HIT_PENETRATION;
            float over = std::min(1.5f, float(shellPen) / std::max(40, effArmor));
            hpLoss = int(pr.shell.damage * (0.85f + 0.3f * over));

            if (isTurret) {
                if (std::fabs(localX) < 0.45f && localZ < 0.2f) {
                    target.modBreech = 0.f;
                    modBits |= MODBIT_BREECH;
                }
                if (std::rand() % 100 < 50) {
                    target.modRing = 0.35f;
                    modBits |= MODBIT_TURRET_RING;
                }
                if (localZ > 0.25f && std::rand() % 100 < 34) {
                    ammoDet = true;
                    modBits |= MODBIT_AMMO_RACK;
                }
                if (target.crew > 1 && std::rand() % 100 < 65) {
                    target.crew--;
                    modBits |= MODBIT_CREW;
                }
            } else {
                if (localZ > 0.45f) {
                    target.modEngine = 0.f;
                    modBits |= MODBIT_ENGINE;
                    if (!target.onFire && std::rand() % 100 < 58) {
                        target.onFire = true;
                        target.fireTimer = 10.f;
                        target.fireAttackerSlot = pr.shooterSlot;
                        modBits |= MODBIT_FIRE;
                    }
                }
                if (std::fabs(localX) > 1.0f && relY < 0.85f) {
                    target.modTracks = 0.f;
                    modBits |= MODBIT_TRACKS;
                }
                if (std::fabs(localZ) < 0.65f && std::fabs(localX) < 0.95f && std::rand() % 100 < 38) {
                    ammoDet = true;
                    modBits |= MODBIT_AMMO_RACK;
                } else if (localZ < -0.35f && target.crew > 1 && std::rand() % 100 < 55) {
                    target.crew--;
                    modBits |= MODBIT_CREW;
                }
            }
            if (modBits != 0) resKind = HIT_CRIT;
        }

        target.hp = std::max(0, target.hp - hpLoss);
        if (ammoDet || target.crew <= 1 || target.hp <= 0) {
            target.hp = 0;
            resKind = HIT_KILL;
        }

        uint16_t rewSilver = 0, rewRp = 0;
        if (resKind == HIT_KILL) {
            rewSilver = 2400; rewRp = 480;
            killTank(targetSlot, pr.shooterSlot, pr.shell.code, ammoDet);
        } else if (resKind == HIT_CRIT || resKind == HIT_PENETRATION) {
            rewSilver = 420; rewRp = 95;
            if (pr.shooterSlot >= 0 && pr.shooterSlot < MAX_NET_TANKS) tanks[pr.shooterSlot].score += 65;
        } else if (resKind == HIT_SPALL) {
            rewSilver = 120; rewRp = 25;
        }

        // Отправляем стрелку 3D Hit Cam пакет!
        if (pr.shooterSlot >= 0 && pr.shooterSlot < MAX_NET_TANKS && tanks[pr.shooterSlot].hasClient) {
            PktHitCam hc;
            hc.targetVehIdx = (uint8_t)target.vehicleIdx;
            hc.result = resKind;
            hc.moduleBits = modBits;
            hc.ammoDetonation = ammoDet ? 1 : 0;
            hc.crewLeft = (uint8_t)target.crew;
            hc.maxCrew = (uint8_t)target.maxCrew;
            hc.hpPercent = (uint8_t)((target.hp * 100) / std::max(1, target.maxHp));
            hc.impactAngleDeg = (uint8_t)impactDeg;
            hc.effectiveArmor = (uint16_t)effArmor;
            hc.shellPen = (uint16_t)shellPen;
            hc.distanceM = (uint16_t)pr.distMeters;
            hc.rewardSilver = rewSilver;
            hc.rewardRp = rewRp;
            hc.localHit[0] = localX; hc.localHit[1] = relY; hc.localHit[2] = localZ;
            hc.localDir[0] = localDir.x; hc.localDir[1] = localDir.y; hc.localDir[2] = localDir.z;
            std::snprintf(hc.zoneName, sizeof(hc.zoneName), "%s", zoneName);
            std::snprintf(hc.shellCode, sizeof(hc.shellCode), "%s", pr.shell.code);
            std::snprintf(hc.targetName, sizeof(hc.targetName), "%s", target.callsign.c_str());
            sendToAddr(tanks[pr.shooterSlot].addr, &hc, sizeof(hc));
        }

        PktFxEvent fx;
        fx.shooterOrTargetSlot = (uint8_t)targetSlot;
        fx.x = pr.pos.x; fx.y = pr.pos.y; fx.z = pr.pos.z;
        if (resKind == HIT_KILL) fx.fxKind = ammoDet ? FX_AMMO_BLOWOFF : FX_TANK_EXPLOSION;
        else if (resKind == HIT_RICOCHET || resKind == HIT_NONPEN) fx.fxKind = FX_SPARK_RICOCHET;
        else fx.fxKind = FX_ARMOR_PEN;
        broadcastToClients(&fx, sizeof(fx));
    }

    void killTank(int victimSlot, int killerSlot, const char* weaponCode, bool ammoDet) {
        SimTank& vic = tanks[victimSlot];
        if (!vic.alive) return;
        vic.alive = false;
        vic.onFire = false;
        vic.repairing = false;
        vic.respawnTimer = vic.isDummy ? 7.0f : 6.0f;
        vic.deaths++;

        if (killerSlot >= 0 && killerSlot < MAX_NET_TANKS && killerSlot != victimSlot) {
            tanks[killerSlot].kills++;
            tanks[killerSlot].score += 300;
        }
        if (!getMaps()[mapIdx].isPoligon) {
            tickets[vic.team] = std::max(0.f, tickets[vic.team] - 60.f);
        }

        PktKillFeed kf;
        kf.victimTeam = (uint8_t)vic.team;
        kf.victimVehIdx = (uint8_t)vic.vehicleIdx;
        kf.ammoDetonation = ammoDet ? 1 : 0;
        std::snprintf(kf.victimName, sizeof(kf.victimName), "%s", vic.callsign.c_str());
        std::snprintf(kf.weaponCode, sizeof(kf.weaponCode), "%s", weaponCode);
        if (killerSlot >= 0 && killerSlot < MAX_NET_TANKS) {
            kf.killerTeam = (uint8_t)tanks[killerSlot].team;
            kf.killerVehIdx = (uint8_t)tanks[killerSlot].vehicleIdx;
            std::snprintf(kf.killerName, sizeof(kf.killerName), "%s", tanks[killerSlot].callsign.c_str());
        } else {
            kf.killerTeam = 1 - vic.team;
            std::snprintf(kf.killerName, sizeof(kf.killerName), "Пожар БК");
        }
        broadcastToClients(&kf, sizeof(kf));
    }

    void updateBot(int slot, float dt, const MapSpec& map) {
        SimTank& bot = tanks[slot];
        if (!bot.alive || bot.isDummy) return;
        const auto& v = getVehicles()[bot.vehicleIdx];

        if (bot.onFire && bot.fpeCount > 0 && bot.fireTimer < 8.2f) {
            bot.onFire = false;
            bot.fpeCount--;
        }
        if (!bot.repairing && (bot.modEngine <= 0.f || bot.modTracks <= 0.f || bot.modBreech <= 0.2f)) {
            bot.repairing = true;
            bot.repairTimer = 5.5f;
        }

        int bestEnemy = -1;
        float bestDist = 300.f;
        for (int i = 0; i < MAX_NET_TANKS; ++i) {
            const SimTank& o = tanks[i];
            if (!o.active || !o.alive || o.team == bot.team) continue;
            float d = std::hypot(o.x - bot.x, o.z - bot.z);
            if (d >= bestDist) continue;
            if (lineConcealedBySmoke(bot.x, bot.z, o.x, o.z)) continue;
            if (!checkSegmentBlocked(map, Vec3(bot.x, bot.y + 1.85f, bot.z), Vec3(o.x, o.y + 1.4f, o.z))) {
                bestDist = d;
                bestEnemy = i;
            }
        }

        bot.aiRethink -= dt;
        if (bot.aiRethink <= 0.f && !map.zones.empty()) {
            bot.aiRethink = 3.5f + float(std::rand() % 25) * 0.1f;
            for (int z = 0; z < 3; ++z) {
                int idx = (slot + z) % 3;
                if (zoneOwner[idx] != bot.team) { bot.aiTargetZone = idx; break; }
            }
        }

        float goalX = map.zones.empty() ? 0.f : map.zones[bot.aiTargetZone % map.zones.size()].x;
        float goalZ = map.zones.empty() ? 0.f : map.zones[bot.aiTargetZone % map.zones.size()].z;
        if (bestEnemy >= 0 && bestDist < 85.f) {
            float ang = std::atan2(tanks[bestEnemy].x - bot.x, tanks[bestEnemy].z - bot.z);
            goalX = bot.x + std::cos(ang) * bot.aiStrafe * 18.f;
            goalZ = bot.z - std::sin(ang) * bot.aiStrafe * 18.f;
        }

        bool canMove = !bot.repairing && bot.modEngine > 0.f && bot.modTracks > 0.f;
        float dGoal = std::hypot(goalX - bot.x, goalZ - bot.z);
        if (canMove && dGoal > 9.f) {
            float desYaw = std::atan2(-(goalX - bot.x), -(goalZ - bot.z));
            bot.hullYaw = moveAngleToward(bot.hullYaw, desYaw, deg2rad(v.hullDegSec) * dt);
            float align = std::cos(wrapAngle(desYaw - bot.hullYaw));
            float maxSpd = (v.fwdKmh / 3.6f) * 0.72f;
            float tgtSpd = align > 0.35f ? maxSpd : maxSpd * 0.25f;
            bot.speed = lerpf(bot.speed, tgtSpd, dt * 3.5f);
        } else {
            bot.speed *= 0.86f;
        }

        float nx = bot.x - std::sin(bot.hullYaw) * bot.speed * dt;
        float nz = bot.z - std::cos(bot.hullYaw) * bot.speed * dt;
        if (resolveMapCollision(map, nx, nz, 2.5f)) {
            bot.hullYaw = wrapAngle(bot.hullYaw + bot.aiStrafe * 0.9f * dt);
        }
        bot.x = nx; bot.z = nz;
        bot.y = getTerrainHeight(mapIdx, bot.x, bot.z);

        if (bestEnemy >= 0) {
            const SimTank& e = tanks[bestEnemy];
            float lead = bestDist / 380.f;
            float ex = e.x - std::sin(e.hullYaw) * e.speed * lead + ((std::rand() % 100 - 50) * 0.022f);
            float ez = e.z - std::cos(e.hullYaw) * e.speed * lead + ((std::rand() % 100 - 50) * 0.022f);
            float ey = e.y + 1.15f;
            float dx = ex - bot.x, dz = ez - bot.z;
            float desTYaw = std::atan2(-dx, -dz);
            bot.turretYaw = moveAngleToward(bot.turretYaw, desTYaw, deg2rad(v.turretDegSec) * bot.modRing * dt);
            float horiz = std::max(1.f, std::hypot(dx, dz));
            float drop = 0.5f * 9.81f * std::pow(horiz / 380.f, 2.f);
            bot.gunPitch = clampf(std::atan2((ey + drop) - (bot.y + 1.72f), horiz), -0.12f, 0.35f);

            if (std::fabs(wrapAngle(desTYaw - bot.turretYaw)) < 0.055f && bot.reloadTimer <= 0.f) {
                fireCannon(slot, 0);
            }
            if (bot.hp < bot.maxHp * 0.42f && bot.smokeCd <= 0.f) {
                deploySmoke(slot);
            }
        } else {
            bot.turretYaw = moveAngleToward(bot.turretYaw, bot.hullYaw, deg2rad(v.turretDegSec) * 0.65f * dt);
            bot.gunPitch = 0.f;
        }
    }

    void tick(float dt) {
        if (sock == INVALID_SOCKET) return;
        uptimeSec += dt;
        pollNetwork();

        const auto& map = getMaps()[mapIdx];

        // Отключаем неактивных более 12 секунд сетевых клиентов
        for (int i = 0; i < MAX_NET_TANKS; ++i) {
            if (tanks[i].active && tanks[i].hasClient && (uptimeSec - tanks[i].lastSeenSec) > 12.0) {
                tanks[i] = SimTank{};
                rebalanceBots();
            }
        }

        if (!map.isPoligon) {
            timeLeftSec = std::max(0.f, timeLeftSec - dt);
        }

        for (int i = (int)smokes.size() - 1; i >= 0; --i) {
            smokes[i].ttl -= dt;
            if (smokes[i].ttl <= 0.f) smokes.erase(smokes.begin() + i);
        }

        for (int i = 0; i < MAX_NET_TANKS; ++i) {
            SimTank& t = tanks[i];
            if (!t.active) continue;
            if (!t.alive) {
                t.respawnTimer -= dt;
                if (t.respawnTimer <= 0.f) {
                    if (t.isDummy) {
                        const auto& v = getVehicles()[t.vehicleIdx];
                        t.alive = true;
                        t.hp = t.maxHp = v.hp;
                        t.crew = t.maxCrew = v.crew;
                        t.x = t.dummyHomeX; t.z = t.dummyHomeZ;
                        t.y = getTerrainHeight(mapIdx, t.x, t.z);
                        t.hullYaw = t.turretYaw = t.dummyHomeYaw;
                        t.modEngine = t.modBreech = t.modTracks = t.modRing = 1.f;
                    } else {
                        spawnTank(i);
                    }
                }
                continue;
            }

            if (t.spawnProtect > 0.f) t.spawnProtect = std::max(0.f, t.spawnProtect - dt);
            if (t.reloadTimer > 0.f) t.reloadTimer = std::max(0.f, t.reloadTimer - dt);
            if (t.smokeCd > 0.f) t.smokeCd = std::max(0.f, t.smokeCd - dt);

            if (t.isDummy && t.dummyMoving) {
                t.dummyPhase += dt * 0.55f;
                t.x = t.dummyHomeX + std::sin(t.dummyPhase) * 34.f;
                t.hullYaw = t.turretYaw = (std::cos(t.dummyPhase) >= 0.f) ? -PI * 0.5f : PI * 0.5f;
                t.y = getTerrainHeight(mapIdx, t.x, t.z);
            }

            if (t.onFire) {
                t.fireTimer -= dt;
                t.hp -= int(55.f * dt + 0.5f);
                if (t.hp <= 0) {
                    killTank(i, t.fireAttackerSlot, "Пожар МТО", false);
                    continue;
                }
                if (t.fireTimer <= 0.f) t.onFire = false;
            }
            if (t.repairing) {
                t.repairTimer -= dt;
                if (t.repairTimer <= 0.f) {
                    t.repairing = false;
                    t.modEngine = t.modBreech = t.modTracks = t.modRing = 1.f;
                }
            }
            if (t.isBot) updateBot(i, dt, map);
        }

        // Движение снарядов и управляемых ПТУР
        for (int i = (int)projectiles.size() - 1; i >= 0; --i) {
            SimProjectile& p = projectiles[i];
            p.ttl -= dt;
            if (p.ttl <= 0.f) { projectiles.erase(projectiles.begin() + i); continue; }

            if (p.guided && p.hasAim) {
                Vec3 desired = (p.aimTarget - p.pos).normalized() * p.simSpeed;
                p.vel = Vec3::lerp(p.vel, desired, dt * 5.2f);
            } else if (!p.guided) {
                p.vel.y -= 9.81f * dt;
            }

            Vec3 nextPos = p.pos + p.vel * dt;
            p.distMeters += (nextPos - p.pos).length() * 1.4f;

            int hitSlot = -1;
            Vec3 seg = nextPos - p.pos;
            float segLenSq = seg.lengthSq() + 1e-6f;
            for (int s = 0; s < MAX_NET_TANKS; ++s) {
                const SimTank& t = tanks[s];
                if (!t.active || !t.alive || s == p.shooterSlot || t.team == p.team) continue;
                Vec3 tCenter(t.x, t.y + 1.15f, t.z);
                float u = clampf(Vec3::dot(tCenter - p.pos, seg) / segLenSq, 0.f, 1.f);
                Vec3 cp = p.pos + seg * u;
                float hDist = std::hypot(cp.x - t.x, cp.z - t.z);
                if (hDist <= 2.35f && cp.y >= t.y - 0.1f && cp.y <= t.y + 2.45f) {
                    float back = std::sqrt(std::max(0.f, 2.25f * 2.25f - hDist * hDist));
                    Vec3 dirN = seg.normalized();
                    p.pos = cp - dirN * back;
                    p.pos.y = clampf(p.pos.y, t.y + 0.25f, t.y + 2.2f);
                    hitSlot = s;
                    break;
                }
            }

            if (hitSlot >= 0) {
                evaluateHit(p, hitSlot);
                projectiles.erase(projectiles.begin() + i);
                continue;
            }

            Vec3 envHit;
            bool isGnd = true;
            if (checkSegmentBlocked(map, p.pos, nextPos, &envHit, &isGnd)) {
                PktFxEvent fx;
                fx.fxKind = FX_DIRT_HIT;
                fx.x = envHit.x; fx.y = envHit.y; fx.z = envHit.z;
                broadcastToClients(&fx, sizeof(fx));
                projectiles.erase(projectiles.begin() + i);
                continue;
            }
            p.pos = nextPos;
        }

        // Точки захвата [A/B/C]
        if (!map.isPoligon && map.zones.size() == 3) {
            int owned[2] = {0, 0};
            for (int z = 0; z < 3; ++z) {
                int inTeam[2] = {0, 0};
                for (int s = 0; s < MAX_NET_TANKS; ++s) {
                    const SimTank& t = tanks[s];
                    if (!t.active || !t.alive) continue;
                    if (std::hypot(t.x - map.zones[z].x, t.z - map.zones[z].z) <= map.zones[z].radius) {
                        inTeam[t.team]++;
                    }
                }
                if (inTeam[0] > 0 && inTeam[1] == 0) {
                    int prev = zoneOwner[z];
                    zoneProg[z] = std::min(100.f, zoneProg[z] + dt * 12.f * std::min(2, inTeam[0]));
                    if (zoneProg[z] >= 100.f) {
                        zoneOwner[z] = 0;
                        if (prev != 0) {
                            for (int s = 0; s < MAX_NET_TANKS; ++s)
                                if (tanks[s].active && tanks[s].alive && tanks[s].team == 0 &&
                                    std::hypot(tanks[s].x - map.zones[z].x, tanks[s].z - map.zones[z].z) <= map.zones[z].radius)
                                    { tanks[s].captures++; tanks[s].score += 200; }
                        }
                    } else if (zoneProg[z] > 0.f && zoneOwner[z] == 1) zoneOwner[z] = -1;
                } else if (inTeam[1] > 0 && inTeam[0] == 0) {
                    int prev = zoneOwner[z];
                    zoneProg[z] = std::max(-100.f, zoneProg[z] - dt * 12.f * std::min(2, inTeam[1]));
                    if (zoneProg[z] <= -100.f) {
                        zoneOwner[z] = 1;
                        if (prev != 1) {
                            for (int s = 0; s < MAX_NET_TANKS; ++s)
                                if (tanks[s].active && tanks[s].alive && tanks[s].team == 1 &&
                                    std::hypot(tanks[s].x - map.zones[z].x, tanks[s].z - map.zones[z].z) <= map.zones[z].radius)
                                    { tanks[s].captures++; tanks[s].score += 200; }
                        }
                    } else if (zoneProg[z] < 0.f && zoneOwner[z] == 0) zoneOwner[z] = -1;
                }
                if (zoneOwner[z] == 0) owned[0]++;
                if (zoneOwner[z] == 1) owned[1]++;
            }
            if (owned[0] > owned[1]) tickets[1] = std::max(0.f, tickets[1] - (owned[0] - owned[1]) * 3.8f * dt);
            if (owned[1] > owned[0]) tickets[0] = std::max(0.f, tickets[0] - (owned[1] - owned[0]) * 3.8f * dt);

            if (tickets[0] <= 0.f || tickets[1] <= 0.f || timeLeftSec <= 0.f) {
                setupMap((mapIdx + 1) % 3); // Ротация карты нового раунда
            }
        }

        // Отправка среза мира клиентам (25 Гц)
        snapshotTimer += dt;
        if (snapshotTimer >= 0.04f) {
            snapshotTimer = 0.f;
            PktWorldSnapshot snap;
            snap.mapIdx = (uint8_t)mapIdx;
            snap.ticketsBlue = (uint16_t)tickets[0];
            snap.ticketsRed  = (uint16_t)tickets[1];
            snap.timeLeftSec = (uint16_t)timeLeftSec;
            for (int z = 0; z < 3; ++z) {
                snap.zoneOwner[z] = (int8_t)zoneOwner[z];
                snap.zoneProg[z]  = (int8_t)zoneProg[z];
            }
            for (int i = 0; i < MAX_NET_TANKS; ++i) {
                const SimTank& t = tanks[i];
                NetTankSnap& ns = snap.tanks[i];
                ns.active = t.active ? 1 : 0;
                if (!t.active) continue;
                ns.alive = t.alive ? 1 : 0;
                ns.team = (uint8_t)t.team;
                ns.vehicleIdx = (uint8_t)t.vehicleIdx;
                ns.isBot = t.isBot ? 1 : 0;
                ns.crew = (uint8_t)t.crew;
                ns.fpe = (uint8_t)t.fpeCount;
                ns.flags = (t.onFire ? 1 : 0) | (t.repairing ? 2 : 0) | (t.isDummy ? 4 : 0);
                ns.hp = (int16_t)t.hp;
                ns.maxHp = (int16_t)t.maxHp;
                ns.x = t.x; ns.y = t.y; ns.z = t.z;
                ns.hullYaw = t.hullYaw; ns.turretYaw = t.turretYaw;
                ns.gunPitch = t.gunPitch; ns.speed = t.speed;
                ns.reloadTimer = t.reloadTimer; ns.smokeCd = t.smokeCd;
                ns.respawnTimer = t.respawnTimer; ns.repairTimer = t.repairTimer;
                ns.modHealth[0] = (uint8_t)(t.modEngine * 100.f);
                ns.modHealth[1] = (uint8_t)(t.modBreech * 100.f);
                ns.modHealth[2] = (uint8_t)(t.modTracks * 100.f);
                ns.modHealth[3] = (uint8_t)(t.modRing * 100.f);
                ns.kills = (uint8_t)t.kills; ns.deaths = (uint8_t)t.deaths;
                ns.captures = (uint8_t)t.captures; ns.score = (uint16_t)t.score;
                std::snprintf(ns.callsign, sizeof(ns.callsign), "%s", t.callsign.c_str());
            }
            int aIdx = 0;
            for (const auto& pr : projectiles) {
                if (pr.guided && aIdx < 4) {
                    snap.atgms[aIdx].active = 1;
                    snap.atgms[aIdx].x = pr.pos.x;
                    snap.atgms[aIdx].y = pr.pos.y;
                    snap.atgms[aIdx].z = pr.pos.z;
                    aIdx++;
                }
            }
            broadcastToClients(&snap, sizeof(snap));
        }
    }
};

} // namespace sf
