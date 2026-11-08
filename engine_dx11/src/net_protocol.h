#pragma once
#include <cstdint>

namespace sf {

constexpr uint32_t NET_MAGIC = 0x53464F31u; // 'SFO1'
constexpr uint16_t DEFAULT_SERVER_PORT = 27015;
constexpr int MAX_NET_TANKS = 16;

enum PacketType : uint8_t {
    PKT_PING           = 1,
    PKT_PONG           = 2,
    PKT_JOIN           = 3,
    PKT_JOIN_ACK       = 4,
    PKT_CLIENT_STATE   = 5,
    PKT_FIRE           = 6,
    PKT_GUIDE_ATGM     = 7,
    PKT_ACTION         = 8, // 1=smoke, 2=repair, 3=fpe, 4=mg, 5=leave
    PKT_WORLD_SNAPSHOT = 9,
    PKT_FX_EVENT       = 10,
    PKT_HIT_CAM        = 11,
    PKT_KILL_FEED      = 12,
    PKT_CHAT           = 13
};

enum HitResultKind : uint8_t {
    HIT_RICOCHET      = 0,
    HIT_NONPEN        = 1,
    HIT_SPALL         = 2,
    HIT_PENETRATION   = 3,
    HIT_CRIT          = 4,
    HIT_KILL          = 5,
    HIT_SPAWN_PROTECT = 6
};

// Битовые флаги поражённых модулей для Hit Cam
enum HitModuleBits : uint8_t {
    MODBIT_ENGINE      = 1 << 0,
    MODBIT_BREECH      = 1 << 1,
    MODBIT_TRACKS      = 1 << 2,
    MODBIT_TURRET_RING = 1 << 3,
    MODBIT_AMMO_RACK   = 1 << 4,
    MODBIT_CREW        = 1 << 5,
    MODBIT_FIRE        = 1 << 6
};

enum FxKind : uint8_t {
    FX_CANNON_FIRE    = 1,
    FX_MG_TRACER      = 2,
    FX_SPARK_RICOCHET = 3,
    FX_ARMOR_PEN      = 4,
    FX_DIRT_HIT       = 5,
    FX_TANK_EXPLOSION = 6,
    FX_AMMO_BLOWOFF   = 7,
    FX_SMOKE_SCREEN   = 8
};

#pragma pack(push, 1)

struct PktHeader {
    uint32_t magic = NET_MAGIC;
    uint8_t  type  = 0;
};

struct PktPing {
    PktHeader hdr{NET_MAGIC, PKT_PING};
    uint32_t  clientTimeMs = 0;
};

struct PktPong {
    PktHeader hdr{NET_MAGIC, PKT_PONG};
    uint32_t  clientTimeMs = 0;
    uint8_t   onlineHumans = 1;
    uint8_t   mapIdx = 0;
};

struct PktJoin {
    PktHeader hdr{NET_MAGIC, PKT_JOIN};
    uint8_t   vehicleIdx = 1;
    uint8_t   preferredMapIdx = 0; // 0..2 Бой, 3 Полигон
    char      callsign[24] = {};
};

struct PktJoinAck {
    PktHeader hdr{NET_MAGIC, PKT_JOIN_ACK};
    uint8_t   assignedSlot = 0;
    uint8_t   team = 0; // 0=blue, 1=red
    uint8_t   mapIdx = 0;
};

struct PktClientState {
    PktHeader hdr{NET_MAGIC, PKT_CLIENT_STATE};
    uint8_t   slot = 0;
    float     x = 0, z = 0;
    float     hullYaw = 0, turretYaw = 0, gunPitch = 0, speed = 0;
};

struct PktFire {
    PktHeader hdr{NET_MAGIC, PKT_FIRE};
    uint8_t   slot = 0;
    uint8_t   shellIdx = 0;
    float     ox = 0, oy = 0, oz = 0;
    float     dx = 0, dy = 0, dz = -1.f;
};

struct PktGuideAtgm {
    PktHeader hdr{NET_MAGIC, PKT_GUIDE_ATGM};
    uint8_t   slot = 0;
    float     tx = 0, ty = 0, tz = 0;
};

struct PktAction {
    PktHeader hdr{NET_MAGIC, PKT_ACTION};
    uint8_t   slot = 0;
    uint8_t   action = 0; // 1=smoke, 2=repair, 3=fpe, 4=mg, 5=leave
    uint8_t   newVehIdx = 0;
    float     ox = 0, oy = 0, oz = 0;
    float     dx = 0, dy = 0, dz = 0;
};

struct NetTankSnap {
    uint8_t  active = 0;
    uint8_t  alive = 0;
    uint8_t  team = 0;
    uint8_t  vehicleIdx = 0;
    uint8_t  isBot = 0;
    uint8_t  crew = 3;
    uint8_t  fpe = 2;
    uint8_t  flags = 0; // bit0=onFire, bit1=repairing, bit2=targetDummy
    int16_t  hp = 2400;
    int16_t  maxHp = 2400;
    float    x = 0, y = 0, z = 0;
    float    hullYaw = 0, turretYaw = 0, gunPitch = 0, speed = 0;
    float    reloadTimer = 0, smokeCd = 0, respawnTimer = 0, repairTimer = 0;
    uint8_t  modHealth[4] = {100, 100, 100, 100}; // eng, breech, tracks, ring (0..100)
    uint8_t  kills = 0, deaths = 0, captures = 0;
    uint16_t score = 0;
    char     callsign[24] = {};
};

struct NetAtgmSnap {
    uint8_t active = 0;
    float   x = 0, y = 0, z = 0;
};

struct PktWorldSnapshot {
    PktHeader   hdr{NET_MAGIC, PKT_WORLD_SNAPSHOT};
    uint8_t     mapIdx = 0;
    uint16_t    ticketsBlue = 2500;
    uint16_t    ticketsRed  = 2500;
    uint16_t    timeLeftSec = 900;
    int8_t      zoneOwner[3] = {-1, -1, -1}; // -1=neutral, 0=blue, 1=red
    int8_t      zoneProg[3]  = {0, 0, 0};    // -100..+100
    NetTankSnap tanks[MAX_NET_TANKS];
    NetAtgmSnap atgms[4];
};

struct PktFxEvent {
    PktHeader hdr{NET_MAGIC, PKT_FX_EVENT};
    uint8_t   fxKind = 0;
    uint8_t   shooterOrTargetSlot = 255;
    uint8_t   shellType = 0;
    float     x = 0, y = 0, z = 0;
    float     dx = 0, dy = 0, dz = 0;
    float     speed = 0;
};

struct PktHitCam {
    PktHeader hdr{NET_MAGIC, PKT_HIT_CAM};
    uint8_t   targetVehIdx = 0;
    uint8_t   result = 0;        // HitResultKind
    uint8_t   moduleBits = 0;    // HitModuleBits
    uint8_t   ammoDetonation = 0;
    uint8_t   crewLeft = 3;
    uint8_t   maxCrew = 3;
    uint8_t   hpPercent = 100;
    uint8_t   impactAngleDeg = 0;
    uint16_t  effectiveArmor = 0;
    uint16_t  shellPen = 0;
    uint16_t  distanceM = 0;
    uint16_t  rewardSilver = 0;
    uint16_t  rewardRp = 0;
    float     localHit[3] = {};
    float     localDir[3] = {};
    char      zoneName[40] = {};
    char      shellCode[20] = {};
    char      targetName[24] = {};
};

struct PktKillFeed {
    PktHeader hdr{NET_MAGIC, PKT_KILL_FEED};
    uint8_t   killerTeam = 0;
    uint8_t   victimTeam = 1;
    uint8_t   killerVehIdx = 0;
    uint8_t   victimVehIdx = 0;
    uint8_t   ammoDetonation = 0;
    char      killerName[24] = {};
    char      victimName[24] = {};
    char      weaponCode[20] = {};
};

struct PktChat {
    PktHeader hdr{NET_MAGIC, PKT_CHAT};
    uint8_t   team = 2; // 0=blue, 1=red, 2=sys
    char      sender[24] = {};
    char      text[96] = {};
};

#pragma pack(pop)

} // namespace sf
