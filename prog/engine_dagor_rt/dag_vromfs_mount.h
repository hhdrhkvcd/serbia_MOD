//
// Dagor Engine 6.5 Runtime — VROMFS (Virtual ROM File System) Mounter & Loose .blk Modding Layer
//
#pragma once
#include "dag_blk_parser.h"
#include <cstdio>
#include <cstdint>
#include <unordered_map>
#ifdef _WIN32
#include <windows.h>
#endif

namespace dagor {

struct VromFsEntry {
    std::string vpath;
    std::string content;
    uint32_t crc32 = 0;
};

class VromFsMount {
public:
    bool vromfsMounted = false;
    bool preferLooseFiles = true;
    uint32_t vromfsVersion = 0;
    std::unordered_map<std::string, VromFsEntry> entries;
    size_t packedFileCount() const { return entries.size(); }

    static bool readDiskFile(const char* path, std::string& out) {
        FILE* f = std::fopen(path, "rb");
        if (!f) return false;
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (sz <= 0 || sz > 32 * 1024 * 1024) { std::fclose(f); return false; }
        out.resize((size_t)sz);
        size_t rd = std::fread(out.data(), 1, (size_t)sz, f);
        std::fclose(f);
        return rd == (size_t)sz;
    }

    bool mountArchive(const char* binPath) {
        entries.clear();
        vromfsMounted = false;
        std::string blob;
        if (!readDiskFile(binPath, blob) || blob.size() < 16) return false;

        const uint8_t* p = (const uint8_t*)blob.data();
        if (std::memcmp(p, "VRFs", 4) != 0) return false;

        uint32_t ver = 0, count = 0, totalCrc = 0;
        std::memcpy(&ver, p + 4, 4);
        std::memcpy(&count, p + 8, 4);
        std::memcpy(&totalCrc, p + 12, 4);
        (void)totalCrc;

        size_t cur = 16;
        for (uint32_t i = 0; i < count && cur + 2 <= blob.size(); ++i) {
            uint16_t plen = 0;
            std::memcpy(&plen, p + cur, 2);
            cur += 2;
            if (cur + plen + 12 > blob.size()) return false;
            std::string vpath((const char*)(p + cur), plen);
            cur += plen;

            uint32_t off = 0, sz = 0, crc = 0;
            std::memcpy(&off, p + cur, 4);
            std::memcpy(&sz,  p + cur + 4, 4);
            std::memcpy(&crc, p + cur + 8, 4);
            cur += 12;

            if ((size_t)off + sz <= blob.size()) {
                VromFsEntry e;
                e.vpath = vpath;
                e.content.assign((const char*)(p + off), sz);
                e.crc32 = crc;
                entries[vpath] = std::move(e);
            }
        }

        vromfsVersion = ver;
        vromfsMounted = !entries.empty();
        return vromfsMounted;
    }

    // Читает виртуальный файл Dagor: сначала проверяет живой .blk на диске (для моддинга!), затем VROMFS
    bool readTextFile(const char* vpath, std::string& outText, bool* outFromLooseDisk = nullptr) const {
        if (preferLooseFiles) {
            std::string diskCandidates[3] = {
                std::string(vpath),
                std::string("prog/gameBase/") + vpath,
                std::string("prog/") + vpath
            };
            for (const auto& dc : diskCandidates) {
                if (readDiskFile(dc.c_str(), outText)) {
                    if (outFromLooseDisk) *outFromLooseDisk = true;
                    return true;
                }
            }
        }
        auto it = entries.find(vpath);
        if (it != entries.end()) {
            outText = it->second.content;
            if (outFromLooseDisk) *outFromLooseDisk = false;
            return true;
        }
        return false;
    }

    bool loadBlk(const char* vpath, DataBlockLite& outBlk, bool* outFromLoose = nullptr) const {
        std::string txt;
        if (!readTextFile(vpath, txt, outFromLoose)) return false;
        return outBlk.loadFromText(txt);
    }

    // Если пользователь скачал только лаунчер + VROMFS, распаковывает .blk на диск для редактирования в Блокноте!
    bool ensureEditableBlkOnDisk(const char* vpath, std::string& outDiskPath) const {
        outDiskPath = std::string("prog/gameBase/") + vpath;
        std::string dummy;
        if (readDiskFile(outDiskPath.c_str(), dummy)) return true;

        auto it = entries.find(vpath);
        if (it == entries.end()) return false;

#ifdef _WIN32
        ::CreateDirectoryA("prog", nullptr);
        ::CreateDirectoryA("prog\\gameBase", nullptr);
        ::CreateDirectoryA("prog\\gameBase\\gamedata", nullptr);
        ::CreateDirectoryA("prog\\gameBase\\gamedata\\units", nullptr);
        ::CreateDirectoryA("prog\\gameBase\\gamedata\\levels", nullptr);
#endif
        FILE* f = std::fopen(outDiskPath.c_str(), "wb");
        if (!f) return false;
        std::fwrite(it->second.content.data(), 1, it->second.content.size(), f);
        std::fclose(f);
        return true;
    }
};

} // namespace dagor
