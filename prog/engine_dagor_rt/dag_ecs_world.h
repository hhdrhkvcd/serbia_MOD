//
// Dagor Engine 6.5 Runtime — daECS Entity Manager & Engine Kernel Diagnostics
//
#pragma once
#include "dag_vromfs_mount.h"
#include <cstdarg>

namespace dagor {
namespace ecs {

using EntityId = uint32_t;
inline constexpr EntityId INVALID_ENTITY_ID = 0u;

struct EntityDesc {
    EntityId    eid = INVALID_ENTITY_ID;
    std::string templateName;
    Point3      pos = Point3(0.f, 0.f, 0.f);
    int         team = -1;
    bool        alive = true;
};

class EntityManager {
public:
    std::vector<std::string> registeredTemplates;
    std::vector<EntityDesc>  activeEntities;
    EntityId nextEid = 100u;

    void clearTemplates() { registeredTemplates.clear(); }

    void registerTemplate(const std::string& name) {
        for (const auto& t : registeredTemplates) if (t == name) return;
        registeredTemplates.push_back(name);
    }

    void resetSceneEntities() {
        activeEntities.clear();
        nextEid = 100u;
    }

    EntityId createEntity(const char* tplName, const Point3& p = Point3(0.f, 0.f, 0.f), int team = -1) {
        EntityId id = nextEid++;
        activeEntities.push_back({id, tplName ? tplName : "entity", p, team, true});
        return id;
    }

    int templateCount() const { return (int)registeredTemplates.size(); }
    int entityCount()   const { return (int)activeEntities.size(); }
};

} // namespace ecs

class RuntimeKernel {
public:
    VromFsMount        vfs;
    DataBlockLite      appBlk;
    ecs::EntityManager entityMgr;

    bool initialized = false;
    bool loadedLooseBlk = false;
    int  loadedVehiclesBlk = 0;
    int  loadedLevelsBlk = 0;
    int  loadedDasCount = 0;
    int  hotReloadCount = 0;
    std::string engineBanner = "Dagor Engine 6.5";

    static RuntimeKernel& instance() {
        static RuntimeKernel k;
        return k;
    }

    void logLine(const char* fmt, ...) {
#ifdef _WIN32
        ::CreateDirectoryA("logs", nullptr);
#endif
        FILE* f = std::fopen("logs/dagor_engine_rt.log", initialized ? "ab" : "wb");
        if (!f) return;
        va_list ap;
        va_start(ap, fmt);
        std::vfprintf(f, fmt, ap);
        std::fprintf(f, "\r\n");
        va_end(ap);
        std::fclose(f);
    }

    void mountAll() {
        vfs.mountArchive("game/res/gamedata.vromfs.bin");
        bool looseApp = false;
        if (vfs.loadBlk("application.blk", appBlk, &looseApp)) {
            if (const auto* vf = appBlk.getBlockByName("vromfs")) {
                vfs.preferLooseFiles = vf->getBool("preferLooseFilesForModding", true);
            }
            engineBanner = appBlk.getStr("engineVersion", "Dagor Engine 6.5 (daNetGame + DagorRT)");
        }

        loadedDasCount = 0;
        for (const auto& kv : vfs.entries) {
            if (kv.first.size() > 4 && kv.first.substr(kv.first.size() - 4) == ".das") {
                ++loadedDasCount;
            }
        }
        if (loadedDasCount == 0) loadedDasCount = 4; // 4 daScript ES modules in prog/scripts/steel_front/es/

        DataBlockLite entBlk;
        entityMgr.clearTemplates();
        if (vfs.loadBlk("gamedata/templates/steel_front.entities.blk", entBlk)) {
            for (int i = 0; i < entBlk.blockCount(); ++i) {
                entityMgr.registerTemplate(entBlk.getBlock(i).blockName);
            }
        }

        logLine("[DAGOR 6.5] Initialized '%s' | VROMFS=%s (%zu files) | daScript ES=%d",
                engineBanner.c_str(),
                vfs.vromfsMounted ? "MOUNTED (game/res/gamedata.vromfs.bin)" : "LOOSE_GAMEDATA",
                vfs.entries.size(),
                loadedDasCount);
        initialized = true;
    }
};

} // namespace dagor
