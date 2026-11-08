//
// Dagor Engine 6.5 Runtime — daECS Entity Manager, Quirrel VM Bridge & Kernel Logger
//
#pragma once
#include "dag_vromfs_mount.h"
#ifdef DAGOR_ENABLE_QUIRREL_VM
#include "dag_quirrel_vm.h"
#endif
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdarg>
#include <cstdio>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace dagor {
namespace ecs {

using EntityId = uint32_t;
constexpr EntityId INVALID_ENTITY_ID = 0;

struct EntityDesc {
    EntityId id = INVALID_ENTITY_ID;
    std::string templateName;
    TMatrix transform = TMatrix::IDENT;
    int team = 0;
    int unitId = 0;
    bool alive = true;
};

class EntityManager {
public:
    std::vector<std::string> registeredTemplates;
    std::vector<EntityDesc>  activeEntities;
    EntityId nextEid = 1001;

    void clearTemplates() { registeredTemplates.clear(); }
    void registerTemplate(const std::string& name) { registeredTemplates.push_back(name); }
    void resetSceneEntities() { activeEntities.clear(); nextEid = 1001; }

    EntityId createEntity(const char* tplName, const TMatrix& tm, int team = 0, int unitId = 0) {
        EntityDesc e;
        e.id = nextEid++;
        e.templateName = tplName ? tplName : "entity";
        e.transform = tm;
        e.team = team;
        e.unitId = unitId;
        e.alive = true;
        activeEntities.push_back(e);
        return e.id;
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
#ifdef DAGOR_ENABLE_QUIRREL_VM
    QuirrelVmHost      quirrelVm;
#endif
    std::vector<std::string> consoleLines;

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

    void pushConsole(const std::string& line) {
        if (consoleLines.size() > 80) consoleLines.erase(consoleLines.begin());
        consoleLines.push_back(line);
    }

    void logLine(const char* fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        pushConsole(buf);

#ifdef _WIN32
        ::CreateDirectoryA("logs", nullptr);
#endif
        FILE* f = std::fopen("logs/dagor_engine_rt.log", initialized ? "ab" : "wb");
        if (!f) return;
        std::fprintf(f, "%s\r\n", buf);
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
        if (loadedDasCount == 0) loadedDasCount = 4;

        DataBlockLite entBlk;
        entityMgr.clearTemplates();
        if (vfs.loadBlk("gamedata/templates/steel_front.entities.blk", entBlk)) {
            for (int i = 0; i < entBlk.blockCount(); ++i) {
                entityMgr.registerTemplate(entBlk.getBlock(i).blockName);
            }
        }

#ifdef DAGOR_ENABLE_QUIRREL_VM
        std::string nutBoot;
        vfs.readTextFile("scripts/ui/dagor_rt_boot.nut", nutBoot);
        quirrelVm.init(nutBoot);
        pushConsole("[QUIRREL VM] Executed scripts/ui/dagor_rt_boot.nut -> " + quirrelVm.bootBanner);
#endif

        logLine("[DAGOR 6.5] Initialized '%s' | VROMFS=%s (%zu files) | daScript ES=%d",
                engineBanner.c_str(),
                vfs.vromfsMounted ? "MOUNTED (game/res/gamedata.vromfs.bin)" : "LOOSE_GAMEDATA",
                vfs.entries.size(),
                loadedDasCount);
        initialized = true;
    }
};

} // namespace dagor
