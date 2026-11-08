#pragma once
#include "d3d11_renderer.h"
#include "game_data.h"

namespace sf {

struct VehicleGpuModel {
    GpuMesh hull;
    GpuMesh turret;
    GpuMesh gun;
    GpuMesh xrayModules;

    void release() {
        hull.release();
        turret.release();
        gun.release();
        xrayModules.release();
    }
};

struct EnvironmentGpuAssets {
    GpuMesh skyDome;
    GpuMesh hangarOutdoor;
    GpuMesh hangarBunker;
    GpuMesh mapTerrain[4];
    GpuMesh unitSphere;
    GpuMesh unitTracer;
    GpuMesh zoneRing;

    void release() {
        skyDome.release();
        hangarOutdoor.release();
        hangarBunker.release();
        for (auto& m : mapTerrain) m.release();
        unitSphere.release();
        unitTracer.release();
        zoneRing.release();
    }
};

inline void addOrganicPineTree(MeshBuilder& mb, float tx, float ty, float tz, float sc, bool snowy = false) {
    const Vec4 bark(0.24f, 0.16f, 0.10f, 1.f);
    const Vec4 folDark  = snowy ? Vec4(0.16f, 0.25f, 0.21f, 1.f) : Vec4(0.10f, 0.20f, 0.10f, 1.f);
    const Vec4 folMid   = snowy ? Vec4(0.20f, 0.30f, 0.25f, 1.f) : Vec4(0.13f, 0.25f, 0.12f, 1.f);
    const Vec4 folLight = snowy ? Vec4(0.26f, 0.36f, 0.31f, 1.f) : Vec4(0.17f, 0.30f, 0.14f, 1.f);

    mb.addCylinder({tx, ty + 1.9f * sc, tz}, 0.28f * sc, 0.14f * sc, 3.8f * sc, 8, 1, bark, 0.92f, 0.02f, 5.f, 0.75f);
    // 4 яруса живой хвои со смещением и разным наклоном
    mb.addCylinder({tx,              ty + 3.4f * sc, tz},              2.35f * sc, 0.55f * sc, 2.4f * sc, 9, 1, folDark,  0.88f, 0.02f, 5.f, 0.78f);
    mb.addCylinder({tx + 0.12f * sc, ty + 5.0f * sc, tz - 0.10f * sc}, 1.95f * sc, 0.42f * sc, 2.3f * sc, 9, 1, folMid,   0.86f, 0.02f, 5.f, 0.86f);
    mb.addCylinder({tx - 0.10f * sc, ty + 6.5f * sc, tz + 0.08f * sc}, 1.48f * sc, 0.26f * sc, 2.1f * sc, 9, 1, folMid,   0.85f, 0.02f, 5.f, 0.92f);
    mb.addCylinder({tx,              ty + 7.9f * sc, tz},              0.95f * sc, 0.05f * sc, 1.9f * sc, 8, 1, folLight, 0.84f, 0.02f, 5.f, 0.98f);
}

inline void buildVehicleMeshes(ID3D11Device* dev, const VehicleSpec& v, VehicleGpuModel& out) {
    out.release();
    const Vec4 armorBase(1.0f, 1.0f, 1.0f, 1.0f);        // Окрашивается камуфляжем в шейдере (tag=1)
    const Vec4 armorDark(0.72f, 0.74f, 0.71f, 1.0f);     // Притемнённые люки, крышки МТО, чехлы (tag=1)
    const Vec4 eraCol(0.88f, 0.90f, 0.86f, 1.0f);        // Динамическая защита Контакт-5 / Реликт / M-19 (tag=7)
    const Vec4 rubberCol(0.09f, 0.09f, 0.10f, 1.0f);     // Чёрная резина бандажей и брызговиков (tag=2)
    const Vec4 trackCol(0.14f, 0.13f, 0.12f, 1.0f);      // Стальные траки с грунтозацепами (tag=2)
    const Vec4 wellDark(0.06f, 0.06f, 0.07f, 1.0f);      // Затенённая ниша за катками (tag=2)
    const Vec4 steelCol(0.20f, 0.21f, 0.23f, 1.0f);      // Оружейная воронёная сталь (tag=3)
    const Vec4 clampCol(0.34f, 0.35f, 0.37f, 1.0f);      // Стальные хомуты термокожуха (tag=3)
    const Vec4 grilleCol(0.05f, 0.05f, 0.06f, 1.0f);     // Радиаторные решётки МТО (tag=3)
    const Vec4 opticCol(0.10f, 0.62f, 0.88f, 1.0f);      // Просветлённая оптика прицелов (tag=6)
    const Vec4 headLight(0.98f, 0.93f, 0.76f, 1.0f);     // Фары (tag=6)

    const bool isEast = (v.arch == ARCH_TSERIES);
    const bool isWest = (v.arch == ARCH_WESTERN || v.arch == ARCH_CHINESE);
    const bool isIfv  = (v.arch == ARCH_IFV && std::strcmp(v.id, "m10") != 0);
    const bool isWw2  = (std::strcmp(v.id, "m10") == 0);
    const bool hasEra = (v.eraName && v.eraName[0] != 0 && std::strcmp(v.eraName, "—") != 0 && std::strcmp(v.eraName, "Нет") != 0);
    const bool hasHeavySkirts = (isEast || isWest) && !isWw2;

    const float hullHalfW = isIfv ? 1.10f : 1.22f;
    const float hullLenF  = isIfv ? -3.15f : (isWest ? -3.55f : -3.36f);
    const float hullLenR  = isIfv ?  3.05f : (isWest ?  3.50f :  3.28f);
    const float deckY     = isIfv ?  1.32f : (isWw2  ?  1.40f :  1.24f);

    // =========================================================================
    // 1. ВЫСОКОДЕТАЛИЗИРОВАННЫЙ КОРПУС И ХОДОВАЯ ЧАСТЬ
    // =========================================================================
    MeshBuilder mb;

    // Тёмная внутренняя стенка ванны корпуса за опорными катками (глубокая тень подвески)
    mb.addChamferBox({0.f, 0.56f, 0.f}, {hullHalfW + 0.04f, 0.34f, (hullLenR - hullLenF) * 0.44f}, 0.02f,
                     wellDark, 0.92f, 0.10f, 2.f, 0.35f);

    // Нижняя броневанна со скошенной НЛД и кормовым листом
    Vec3 hullLower[8] = {
        {-hullHalfW, 0.38f, hullLenF + 0.68f}, { hullHalfW, 0.38f, hullLenF + 0.68f},
        { hullHalfW, 0.40f, hullLenR - 0.40f}, {-hullHalfW, 0.40f, hullLenR - 0.40f},
        {-hullHalfW, 0.85f, hullLenF},         { hullHalfW, 0.85f, hullLenF},
        { hullHalfW, 0.89f, hullLenR},         {-hullHalfW, 0.89f, hullLenR}
    };
    mb.addPrism8(hullLower, armorBase, 0.52f, 0.24f, 1.f, 0.80f);

    // Верхняя часть корпуса с пологой ВЛД (~68° наклона!)
    Vec3 hullUpper[8] = {
        {-hullHalfW,         0.85f, hullLenF},          { hullHalfW,         0.85f, hullLenF},
        { hullHalfW,         0.89f, hullLenR},          {-hullHalfW,         0.89f, hullLenR},
        {-hullHalfW + 0.08f, deckY, hullLenF + 1.42f},  { hullHalfW - 0.08f, deckY, hullLenF + 1.42f},
        { hullHalfW - 0.06f, deckY + 0.05f, hullLenR - 0.16f}, {-hullHalfW + 0.06f, deckY + 0.05f, hullLenR - 0.16f}
    };
    mb.addPrism8(hullUpper, armorBase, 0.48f, 0.26f, 1.f, 1.0f);

    // Буксирные проушины и запасные траки на НЛД
    for (int side = -1; side <= 1; side += 2) {
        mb.addChamferBox({side * 0.78f, 0.64f, hullLenF + 0.28f}, {0.07f, 0.09f, 0.12f}, 0.02f, steelCol, 0.48f, 0.55f, 3.f, 0.85f);
    }
    for (int st = -2; st <= 2; ++st) {
        Mat4 lfpXf = Mat4::rotationX(-0.72f) * Mat4::translation(st * 0.24f, 0.58f, hullLenF + 0.36f);
        mb.addChamferBox({0,0,0}, {0.09f, 0.025f, 0.16f}, 0.005f, trackCol, 0.78f, 0.42f, 2.f, 0.8f, &lfpXf);
    }

    // V-образный волноотражательный щиток на ВЛД (характерная деталь Т-72 / Т-80 / M-84)
    if (isEast || isIfv) {
        for (int side = -1; side <= 1; side += 2) {
            Mat4 ribXf = Mat4::rotationY(side * 0.26f) * Mat4::rotationX(0.28f) *
                         Mat4::translation(side * 0.52f, 1.06f, hullLenF + 0.76f);
            mb.addChamferBox({0,0,0}, {0.54f, 0.045f, 0.025f}, 0.006f, armorDark, 0.50f, 0.28f, 1.f, 0.95f, &ribXf);
        }
    }

    // Надгусеничные полки, передние/задние грязевые крылья, экраны ДЗ и гусеничный движитель
    for (int side = -1; side <= 1; side += 2) {
        float sx = side * (hullHalfW + 0.30f);
        float tx = side * (hullHalfW + 0.28f);

        // Верхняя надгусеничная полка во всю длину
        mb.addChamferBox({sx, 0.97f, 0.0f}, {0.27f, 0.055f, (hullLenR - hullLenF) * 0.50f}, 0.02f,
                         armorBase, 0.50f, 0.25f, 1.f, 0.96f);

        // Скруглённый передний грязевой щиток с резиновым брызговиком (полностью закрывает ленивец сверху-спереди!)
        Mat4 fFender = Mat4::rotationX(0.58f) * Mat4::translation(sx, 0.84f, hullLenF - 0.06f);
        mb.addChamferBox({0,0,0}, {0.27f, 0.04f, 0.24f}, 0.015f, armorBase, 0.50f, 0.25f, 1.f, 0.92f, &fFender);
        Mat4 fFlap   = Mat4::rotationX(0.15f) * Mat4::translation(sx, 0.62f, hullLenF - 0.19f);
        mb.addChamferBox({0,0,0}, {0.26f, 0.14f, 0.02f}, 0.004f, rubberCol, 0.88f, 0.05f, 2.f, 0.80f, &fFlap);

        // Задний грязевой щиток с габаритным стоп-сигналом
        Mat4 rFender = Mat4::rotationX(-0.55f) * Mat4::translation(sx, 0.85f, hullLenR + 0.05f);
        mb.addChamferBox({0,0,0}, {0.27f, 0.04f, 0.22f}, 0.015f, armorBase, 0.50f, 0.25f, 1.f, 0.90f, &rFender);
        mb.addChamferBox({sx, 0.94f, hullLenR + 0.12f}, {0.06f, 0.03f, 0.02f}, 0.004f, Vec4(0.88f, 0.14f, 0.10f, 1.f), 0.2f, 0.2f, 6.f);

        // Бортовые экраны: тяжёлые блоки ДЗ «Реликт»/«Контакт-5» спереди + резинотканевые секции сзади
        int skirtPanels = isWest ? 5 : 6;
        float zSpan = (hullLenR - hullLenF) * 0.90f;
        float panelLen = zSpan / skirtPanels;
        for (int p = 0; p < skirtPanels; ++p) {
            float pz = hullLenF + 0.36f + (p + 0.5f) * panelLen;
            bool heavyFront = hasHeavySkirts && (p < 3);
            mb.addChamferBox({side * (hullHalfW + 0.54f), heavyFront ? 0.78f : 0.76f, pz},
                             {heavyFront ? 0.065f : 0.032f, heavyFront ? 0.24f : 0.19f, panelLen * 0.47f},
                             0.02f, heavyFront ? eraCol : rubberCol, heavyFront ? 0.48f : 0.86f,
                             heavyFront ? 0.26f : 0.08f, heavyFront ? 7.f : 2.f, 0.92f);
            // Стальные петли крепления экрана
            mb.addChamferBox({side * (hullHalfW + 0.56f), 0.96f, pz}, {0.04f, 0.03f, panelLen * 0.36f},
                             0.008f, steelCol, 0.46f, 0.52f, 3.f, 0.90f);
        }

        // Гусеничный обвод вокруг ленивца, катков и ведущего колеса + грунтозацепы
        int numWheels = isWest ? 7 : 6;
        float zFirstW = hullLenF + 0.86f;
        float zLastW  = hullLenR - 0.86f;
        float zIdler  = hullLenF + 0.34f;
        float zSprock = hullLenR - 0.34f;

        // Нижняя и верхняя ветви траков
        mb.addChamferBox({tx, 0.055f, (zFirstW + zLastW) * 0.5f}, {0.26f, 0.048f, (zLastW - zFirstW) * 0.52f}, 0.012f,
                         trackCol, 0.76f, 0.42f, 2.f, 0.62f);
        mb.addChamferBox({tx, 0.84f, 0.0f}, {0.26f, 0.042f, (zSprock - zIdler) * 0.50f}, 0.012f,
                         trackCol, 0.76f, 0.42f, 2.f, 0.72f);

        // Передняя и задняя наклонные ветви + огибание ленивца и звёздочки траками (никаких голых цилиндров!)
        Mat4 frontSlope = Mat4::rotationX(-0.68f) * Mat4::translation(tx, 0.29f, (zIdler + zFirstW) * 0.5f - 0.04f);
        mb.addChamferBox({0,0,0}, {0.26f, 0.046f, 0.38f}, 0.012f, trackCol, 0.76f, 0.42f, 2.f, 0.74f, &frontSlope);
        mb.addCylinder({tx, 0.56f, zIdler}, 0.305f, 0.305f, 0.51f, 16, 0, trackCol, 0.76f, 0.42f, 2.f, 0.76f);

        Mat4 rearSlope = Mat4::rotationX(0.68f) * Mat4::translation(tx, 0.30f, (zSprock + zLastW) * 0.5f + 0.04f);
        mb.addChamferBox({0,0,0}, {0.26f, 0.046f, 0.38f}, 0.012f, trackCol, 0.76f, 0.42f, 2.f, 0.74f, &rearSlope);
        mb.addCylinder({tx, 0.57f, zSprock}, 0.315f, 0.315f, 0.51f, 16, 0, trackCol, 0.76f, 0.42f, 2.f, 0.76f);

        // Рельефные грунтозацепы на переднем обводе гусеницы
        for (int cl = 0; cl < 5; ++cl) {
            float a = -0.9f + cl * 0.42f;
            Vec3 cp(tx, 0.56f + std::sin(a) * 0.31f, zIdler - std::cos(a) * 0.31f);
            Mat4 cXf = Mat4::rotationX(a) * Mat4::translation(cp.x, cp.y, cp.z);
            mb.addChamferBox({0,0,0}, {0.265f, 0.018f, 0.035f}, 0.004f, steelCol, 0.55f, 0.55f, 3.f, 0.8f, &cXf);
        }

        // Опорные катки с резиновым бандажом, вогнутым штампованным диском, ступицей и балансиром
        for (int w = 0; w < numWheels; ++w) {
            float wz = zFirstW + (float(w) / (numWheels - 1)) * (zLastW - zFirstW);
            float rWheel = isIfv ? 0.30f : 0.35f;
            float wy = rWheel + 0.055f;
            // Резиновый бандаж
            mb.addCylinder({tx, wy, wz}, rWheel, rWheel, 0.46f, 20, 0, rubberCol, 0.88f, 0.06f, 2.f, 0.70f);
            // Броневой диск катка и внутреннее углубление
            mb.addCylinder({tx + side * 0.02f, wy, wz}, rWheel * 0.75f, rWheel * 0.70f, 0.48f, 18, 0, armorBase, 0.48f, 0.28f, 1.f, 0.84f);
            mb.addCylinder({tx + side * 0.13f, wy, wz}, rWheel * 0.52f, rWheel * 0.48f, 0.28f, 16, 0, armorDark, 0.52f, 0.32f, 1.f, 0.68f);
            // Центральный колпак ступицы и болты
            mb.addCylinder({tx + side * 0.17f, wy, wz}, 0.105f, 0.088f, 0.28f, 12, 0, steelCol, 0.40f, 0.60f, 3.f, 0.92f);
        }

        // Ступицы ленивца и зубчатого ведущего колеса
        mb.addCylinder({tx + side * 0.04f, 0.56f, zIdler},  0.22f, 0.20f, 0.54f, 14, 0, armorDark, 0.48f, 0.35f, 1.f, 0.82f);
        mb.addCylinder({tx + side * 0.04f, 0.57f, zSprock}, 0.24f, 0.22f, 0.55f, 14, 0, steelCol,  0.42f, 0.58f, 3.f, 0.82f);
    }

    // Плотная мозаика блоков ДЗ (Контакт-5 / Реликт / ERO-19) на ВЛД с тёмными швами
    if (hasEra) {
        Mat4 glacisXf = Mat4::rotationX(0.28f) * Mat4::translation(0.f, 1.07f, hullLenF + 0.72f);
        for (int row = 0; row < 2; ++row) {
            for (int col = -3; col <= 3; ++col) {
                if (row == 1 && col == 0) continue; // смотровой канал мехвода
                Mat4 tileXf = Mat4::translation(col * 0.31f, 0.032f, (row - 0.5f) * 0.33f) * glacisXf;
                mb.addChamferBox({0,0,0}, {0.145f, 0.036f, 0.155f}, 0.014f, eraCol, 0.45f, 0.28f, 7.f, 0.96f, &tileXf);
            }
        }
    }

    // Люк и триплексы механика-водителя + фары с защитными дугами
    mb.addChamferBox({0.0f, deckY + 0.04f, hullLenF + 1.42f}, {0.24f, 0.055f, 0.16f}, 0.02f, armorDark, 0.48f, 0.30f, 1.f);
    mb.addChamferBox({0.0f, deckY + 0.06f, hullLenF + 1.27f}, {0.15f, 0.024f, 0.025f}, 0.004f, opticCol, 0.1f, 0.9f, 6.f);
    for (int side = -1; side <= 1; side += 2) {
        mb.addChamferBox({side * 1.12f, 1.06f, hullLenF + 0.42f}, {0.13f, 0.11f, 0.12f}, 0.015f, steelCol, 0.45f, 0.55f, 3.f, 0.9f);
        mb.addCylinder({side * 1.12f, 1.06f, hullLenF + 0.38f}, 0.082f, 0.072f, 0.11f, 14, 2, steelCol, 0.42f, 0.55f, 3.f);
        mb.addCylinder({side * 1.12f, 1.06f, hullLenF + 0.32f}, 0.064f, 0.064f, 0.02f, 14, 2, headLight, 0.1f, 0.1f, 6.f);
    }

    // Крыша МТО с детализированными жалюзи радиаторов и боковым выхлопным коллектором
    mb.addChamferBox({0.f, deckY + 0.06f, hullLenR - 1.06f}, {1.06f, 0.05f, 0.88f}, 0.03f, armorDark, 0.52f, 0.26f, 1.f);
    for (int g = -1; g <= 1; g += 2) {
        mb.addChamferBox({g * 0.52f, deckY + 0.115f, hullLenR - 1.12f}, {0.39f, 0.016f, 0.54f}, 0.005f, grilleCol, 0.90f, 0.35f, 3.f, 0.48f);
        for (int sl = -3; sl <= 3; ++sl) {
            mb.addChamferBox({g * 0.52f, deckY + 0.132f, hullLenR - 1.12f + sl * 0.072f}, {0.38f, 0.008f, 0.015f}, 0.002f, steelCol, 0.55f, 0.45f, 3.f);
        }
    }
    // Боковой выхлопной патрубок по левому борту (Т-72 / M-84 / Т-90)
    if (isEast) {
        mb.addChamferBox({-(hullHalfW + 0.34f), 1.05f, hullLenR - 1.25f}, {0.12f, 0.08f, 0.36f}, 0.02f, steelCol, 0.65f, 0.45f, 3.f, 0.75f);
    }

    // Кормовые топливные бочки (200 л) с рёбрами жёсткости и бревно самовытаскивания
    if (v.hasFuelDrums) {
        for (int side = -1; side <= 1; side += 2) {
            mb.addCylinder({side * 0.68f, 1.18f, hullLenR + 0.22f}, 0.23f, 0.23f, 0.90f, 18, 0, armorDark, 0.52f, 0.26f, 1.f, 0.90f);
            mb.addCylinder({side * 0.52f, 1.18f, hullLenR + 0.22f}, 0.242f, 0.242f, 0.03f, 18, 0, steelCol, 0.45f, 0.55f, 3.f);
            mb.addCylinder({side * 0.84f, 1.18f, hullLenR + 0.22f}, 0.242f, 0.242f, 0.03f, 18, 0, steelCol, 0.45f, 0.55f, 3.f);
            mb.addChamferBox({side * 0.68f, 1.00f, hullLenR + 0.05f}, {0.36f, 0.04f, 0.16f}, 0.01f, steelCol, 0.5f, 0.5f, 3.f);
        }
    }
    if (v.hasLog) {
        mb.addCylinder({0.f, 0.94f, hullLenR + 0.12f}, 0.085f, 0.080f, 2.52f, 12, 0,
                       Vec4(0.36f, 0.25f, 0.15f, 1.f), 0.90f, 0.02f, 5.f, 0.88f);
    }
    if (isEast && v.hasRcws) {
        for (int bar = 0; bar < 4; ++bar) {
            mb.addChamferBox({0.f, 0.66f + bar * 0.12f, hullLenR + 0.10f}, {1.36f, 0.014f, 0.028f}, 0.004f, steelCol, 0.5f, 0.5f, 3.f);
        }
    }
    out.hull = mb.upload(dev);

    // =========================================================================
    // 2. СКУЛЬПТУРНАЯ БАШНЯ С ДЗ, ПРИЦЕЛАМИ, ЗПУ И ДЫМОВЫМИ ГРАНАТОМЁТАМИ
    // =========================================================================
    mb.clear();
    const float turBaseY = deckY + 0.04f;
    // Затенённый погон башни
    mb.addCylinder({0.f, turBaseY + 0.03f, -0.06f}, 1.14f, 1.14f, 0.08f, 24, 1, wellDark, 0.85f, 0.2f, 2.f, 0.45f);

    if (isEast) {
        // Приплюснутый литой/сварной купол Т-72Б3 / Т-80БВМ / Т-90М / M-84AS1 / M-84AS2 «Војвода»
        mb.addCylinder({0.f, turBaseY + 0.12f, -0.08f}, 1.24f, 1.18f, 0.18f, 28, 1, armorBase, 0.48f, 0.26f, 1.f, 0.88f);
        mb.addEllipsoid({0.f, turBaseY + 0.15f, -0.08f}, {1.32f, 0.54f, 1.40f}, 16, 28, true, armorBase, 0.46f, 0.28f, 1.f, 1.0f);

        // Бортовые ящики ЗИП и труба ОПВТ на корме башни
        for (int side = -1; side <= 1; side += 2) {
            mb.addChamferBox({side * 1.22f, turBaseY + 0.30f, 0.36f}, {0.14f, 0.18f, 0.42f}, 0.03f, armorDark, 0.52f, 0.24f, 1.f, 0.92f);
        }
        if (v.hasBustle) {
            mb.addChamferBox({0.f, turBaseY + 0.35f, 1.22f}, {0.94f, 0.25f, 0.48f}, 0.05f, armorBase, 0.48f, 0.26f, 1.f, 0.95f);
        } else {
            mb.addCylinder({0.f, turBaseY + 0.42f, 1.28f}, 0.11f, 0.11f, 1.75f, 14, 0, armorDark, 0.52f, 0.28f, 1.f, 0.90f);
        }

        // Плотный клиновидный шеврон ДЗ «Контакт-5» / «Реликт» / «ERO-19» с резиновыми фартуками
        if (hasEra) {
            for (int side = -1; side <= 1; side += 2) {
                for (int k = 0; k < 5; ++k) {
                    float ang = side * (0.24f + k * 0.21f);
                    float rx = std::sin(ang) * 1.32f;
                    float rz = -0.08f - std::cos(ang) * 1.36f;
                    Mat4 wXf = Mat4::rotationY(-ang) * Mat4::translation(rx, turBaseY + 0.30f, rz);
                    Mat4 upW = Mat4::rotationX(0.46f) * wXf;
                    Mat4 dnW = Mat4::rotationX(-0.42f) * Mat4::translation(0.f, -0.14f, 0.f) * wXf;
                    mb.addChamferBox({0,0,0}, {0.155f, 0.135f, 0.10f}, 0.02f, eraCol, 0.44f, 0.30f, 7.f, 0.98f, &upW);
                    mb.addChamferBox({0,0,0}, {0.155f, 0.115f, 0.09f}, 0.02f, eraCol, 0.44f, 0.30f, 7.f, 0.92f, &dnW);
                }
            }
        }
    } else if (isWest) {
        Vec3 turCore[8] = {
            {-1.28f, turBaseY,        -1.15f}, { 1.28f, turBaseY,        -1.15f},
            { 1.24f, turBaseY,         1.88f}, {-1.24f, turBaseY,         1.88f},
            {-1.18f, turBaseY + 0.68f,-1.02f}, { 1.18f, turBaseY + 0.68f,-1.02f},
            { 1.15f, turBaseY + 0.68f, 1.80f}, {-1.15f, turBaseY + 0.68f, 1.80f}
        };
        mb.addPrism8(turCore, armorBase, 0.46f, 0.26f, 1.f, 1.0f);

        if (v.wedgeTurret) {
            for (int side = -1; side <= 1; side += 2) {
                float sx0 = side * 0.32f, sx1 = side * 1.28f;
                Vec3 cheek[8] = {
                    {std::min(sx0, sx1), turBaseY + 0.05f, -1.80f}, {std::max(sx0, sx1), turBaseY + 0.05f, -1.80f},
                    {std::max(sx0, sx1), turBaseY + 0.02f, -1.05f}, {std::min(sx0, sx1), turBaseY + 0.02f, -1.05f},
                    {std::min(sx0, sx1), turBaseY + 0.65f, -1.54f}, {std::max(sx0, sx1), turBaseY + 0.65f, -1.26f},
                    {std::max(sx0, sx1), turBaseY + 0.68f, -1.02f}, {std::min(sx0, sx1), turBaseY + 0.68f, -1.02f}
                };
                if (side < 0) { cheek[4].z = -1.26f; cheek[5].z = -1.54f; }
                mb.addPrism8(cheek, hasEra ? eraCol : armorBase, 0.44f, 0.28f, hasEra ? 7.f : 1.f, 0.98f);
            }
        }
        mb.addChamferBox({-0.45f, turBaseY + 0.70f, 1.15f}, {0.38f, 0.025f, 0.48f}, 0.01f, armorDark, 0.52f, 0.26f, 1.f);
        mb.addChamferBox({ 0.45f, turBaseY + 0.70f, 1.15f}, {0.38f, 0.025f, 0.48f}, 0.01f, armorDark, 0.52f, 0.26f, 1.f);
        // Кормовая корзина для имущества НАТО
        mb.addChamferBox({0.f, turBaseY + 0.38f, 2.04f}, {1.05f, 0.22f, 0.16f}, 0.02f, steelCol, 0.55f, 0.45f, 3.f, 0.88f);
    } else if (isIfv) {
        Vec3 ifvTur[8] = {
            {-0.88f, turBaseY,        -0.88f}, { 0.88f, turBaseY,        -0.88f},
            { 0.84f, turBaseY,         0.92f}, {-0.84f, turBaseY,         0.92f},
            {-0.68f, turBaseY + 0.50f,-0.65f}, { 0.68f, turBaseY + 0.50f,-0.65f},
            { 0.68f, turBaseY + 0.52f, 0.82f}, {-0.68f, turBaseY + 0.52f, 0.82f}
        };
        mb.addPrism8(ifvTur, armorBase, 0.48f, 0.25f, 1.f, 1.0f);

        if (v.hasAtgmRails) {
            mb.addChamferBox({0.62f, turBaseY + 0.62f, 0.15f}, {0.26f, 0.06f, 0.22f}, 0.015f, steelCol, 0.45f, 0.5f, 3.f);
            for (int r = -1; r <= 1; r += 2) {
                mb.addCylinder({0.62f + r * 0.14f, turBaseY + 0.76f, 0.10f}, 0.075f, 0.065f, 0.86f, 14, 2, eraCol, 0.45f, 0.30f, 7.f, 0.95f);
                mb.addCylinder({0.62f + r * 0.14f, turBaseY + 0.76f, -0.38f}, 0.01f, 0.065f, 0.16f, 14, 2, Vec4(0.75f, 0.20f, 0.16f, 1.f), 0.4f, 0.3f, 3.f);
            }
        }
    } else {
        Vec3 tdTur[8] = {
            {-1.05f, turBaseY,        -0.95f}, { 1.05f, turBaseY,        -0.95f},
            { 0.98f, turBaseY,         1.35f}, {-0.98f, turBaseY,         1.35f},
            {-0.88f, turBaseY + 0.66f,-0.80f}, { 0.88f, turBaseY + 0.66f,-0.80f},
            { 0.86f, turBaseY + 0.66f, 1.48f}, {-0.86f, turBaseY + 0.66f, 1.48f}
        };
        mb.addPrism8(tdTur, armorBase, 0.52f, 0.24f, 1.f, 1.0f);
    }

    // Командирская башенка, люк наводчика, тепловизионный прицел «Сосна-У» и панорама CITV
    float roofY = turBaseY + (isIfv ? 0.50f : (isEast ? 0.52f : 0.66f));
    mb.addCylinder({ 0.52f, roofY + 0.07f, 0.18f}, 0.28f, 0.26f, 0.14f, 18, 1, armorDark, 0.48f, 0.28f, 1.f);
    mb.addCylinder({-0.48f, roofY + 0.04f, 0.22f}, 0.24f, 0.23f, 0.08f, 16, 1, armorDark, 0.50f, 0.26f, 1.f);

    // Бронированный кожух прицела «Сосна-У» / GPS с оптическим окном
    mb.addChamferBox({-0.54f, roofY + 0.13f, -0.32f}, {0.16f, 0.12f, 0.20f}, 0.02f, armorDark, 0.46f, 0.30f, 1.f);
    mb.addChamferBox({-0.54f, roofY + 0.13f, -0.53f}, {0.11f, 0.075f, 0.015f}, 0.004f, opticCol, 0.08f, 0.9f, 6.f);

    if (v.hasRcws || isWest) {
        mb.addCylinder({0.42f, roofY + 0.22f, 0.54f}, 0.16f, 0.14f, 0.26f, 16, 1, armorDark, 0.46f, 0.30f, 1.f);
        mb.addChamferBox({0.42f, roofY + 0.25f, 0.40f}, {0.095f, 0.065f, 0.02f}, 0.004f, opticCol, 0.08f, 0.9f, 6.f);
    }

    // Реалистичный зенитный пулемёт 12.7-мм (НСВТ / Корд / M-15 ДУБМ) на командирской турели:
    // вертлюг + патронная коробка + ствольная коробка + ствол с пламегасителем (аккуратные пропорции!)
    mb.addCylinder({0.52f, roofY + 0.18f, 0.08f}, 0.05f, 0.04f, 0.14f, 10, 1, steelCol, 0.42f, 0.58f, 3.f);
    mb.addChamferBox({0.52f, roofY + 0.27f, 0.10f}, {0.075f, 0.075f, 0.20f}, 0.015f, steelCol, 0.38f, 0.62f, 3.f);
    mb.addChamferBox({0.64f, roofY + 0.26f, 0.12f}, {0.055f, 0.085f, 0.13f}, 0.01f, armorDark, 0.50f, 0.30f, 1.f); // короб с лентой
    mb.addCylinder({0.52f, roofY + 0.28f, -0.32f}, 0.022f, 0.017f, 0.52f, 10, 2, steelCol, 0.35f, 0.68f, 3.f);
    mb.addCylinder({0.52f, roofY + 0.28f, -0.60f}, 0.028f, 0.020f, 0.07f, 10, 2, steelCol, 0.35f, 0.68f, 3.f);

    // Аккуратные кластеры дымовых гранатомётов 902Б «Туча» по бортам башни
    for (int side = -1; side <= 1; side += 2) {
        for (int s = 0; s < 4; ++s) {
            Mat4 smXf = Mat4::rotationX(0.38f) * Mat4::rotationY(side * 0.28f) *
                        Mat4::translation(side * (isIfv ? 0.88f : 1.24f), turBaseY + 0.34f + (s % 2) * 0.09f, 0.02f + (s / 2) * 0.12f);
            mb.addCylinder({0,0,0}, 0.038f, 0.038f, 0.20f, 10, 2, steelCol, 0.44f, 0.52f, 3.f, 0.9f, true, &smXf);
        }
    }

    // Датчик ветра СУО и гибкая штыревая антенна на корме башни
    mb.addCylinder({0.0f, roofY + 0.22f, 0.88f}, 0.022f, 0.016f, 0.36f, 8, 1, steelCol, 0.42f, 0.58f, 3.f);
    mb.addCylinder({-0.78f, roofY + 0.82f, 0.78f}, 0.010f, 0.004f, 1.65f, 6, 1, steelCol, 0.40f, 0.62f, 3.f);

    out.turret = mb.upload(dev);

    // =========================================================================
    // 3. ОРУДИЕ С БРЕЗЕНТОВОЙ МАСКОЙ, СТЯЖНЫМИ ХОМУТАМИ И ЭЖЕКТОРОМ
    // =========================================================================
    mb.clear();
    // Бронированная амбразура и гофрированный брезентовый чехол маски орудия
    mb.addChamferBox({0.f, 0.f, -0.38f}, {0.32f, 0.21f, 0.30f}, 0.045f, armorDark, 0.62f, 0.14f, 1.f, 0.95f);
    mb.addCylinder({0.f, 0.f, -0.72f}, 0.15f, 0.12f, 0.24f, 18, 2, rubberCol, 0.82f, 0.06f, 2.f, 0.90f);
    // Спаренный 7.62-мм ПКТМ в бронекожухе (короткий аккуратный дульный срез справа от маски)
    mb.addCylinder({0.21f, 0.01f, -0.72f}, 0.016f, 0.014f, 0.24f, 8, 2, steelCol, 0.36f, 0.68f, 3.f);

    float gunR   = isIfv ? 0.048f : (isWw2 ? 0.068f : 0.086f);
    float gunLen = isIfv ? 2.45f  : (isWest ? 4.55f : 4.28f);

    // Казённый термозащитный кожух ствола
    mb.addCylinder({0.f, 0.f, -1.28f}, gunR * 1.24f, gunR * 1.08f, 1.25f, 20, 2, armorBase, 0.46f, 0.30f, 1.f);
    // Эксцентриковый эжектор пороховых газов
    if (!isIfv && !isWw2) {
        mb.addCylinder({0.f, 0.018f, -2.28f}, gunR * 1.56f, gunR * 1.52f, 0.68f, 22, 2, armorDark, 0.44f, 0.34f, 1.f);
    }
    // Передняя секция термокожуха + стальные стяжные кольца
    mb.addCylinder({0.f, 0.f, -0.72f - gunLen * 0.5f}, gunR, gunR * 0.92f, gunLen, 20, 2, armorBase, 0.45f, 0.32f, 1.f);
    if (!isIfv) {
        for (int b = 0; b < 4; ++b) {
            float bz = -1.45f - b * 0.88f;
            mb.addCylinder({0.f, 0.f, bz}, gunR * 1.07f, gunR * 1.07f, 0.035f, 20, 2, clampCol, 0.35f, 0.68f, 3.f);
        }
    }
    // Дульное утолщение, датчик УУИ и тёмный канал ствола
    mb.addCylinder({0.f, 0.f, -0.72f - gunLen - 0.06f}, gunR * 1.12f, gunR * 1.08f, 0.14f, 20, 2, steelCol, 0.34f, 0.70f, 3.f);
    mb.addCylinder({0.f, 0.f, -0.72f - gunLen - 0.135f}, gunR * 0.70f, gunR * 0.70f, 0.02f, 18, 2, Vec4(0.01f,0.01f,0.01f,1.f), 0.95f, 0.05f, 3.f);
    if (!isIfv && !isWw2) {
        mb.addChamferBox({0.055f, gunR * 1.26f, -0.72f - gunLen - 0.04f}, {0.022f, 0.032f, 0.045f}, 0.005f, steelCol, 0.38f, 0.65f, 3.f);
    }
    out.gun = mb.upload(dev);

    // =========================================================================
    // 4. ВНУТРЕННИЕ МОДУЛИ X-RAY
    // =========================================================================
    mb.clear();
    mb.addChamferBox({0.f, 0.76f, 1.75f}, {0.68f, 0.34f, 0.82f}, 0.05f, Vec4(0.95f, 0.48f, 0.12f, 0.92f), 0.3f, 0.5f, 8.f);
    mb.addCylinder({0.f, 0.72f, 2.65f}, 0.36f, 0.36f, 0.78f, 14, 0, Vec4(0.85f, 0.38f, 0.14f, 0.92f), 0.3f, 0.5f, 8.f);
    if (isEast) {
        mb.addCylinder({0.f, 0.52f, -0.08f}, 0.92f, 0.92f, 0.28f, 20, 1, Vec4(0.96f, 0.22f, 0.18f, 0.94f), 0.3f, 0.5f, 8.f);
    } else {
        mb.addChamferBox({0.f, turBaseY + 0.34f, 1.18f}, {0.78f, 0.22f, 0.42f}, 0.03f, Vec4(0.96f, 0.22f, 0.18f, 0.94f), 0.3f, 0.5f, 8.f);
    }
    mb.addChamferBox({0.f, 1.66f, -0.15f}, {0.24f, 0.20f, 0.55f}, 0.03f, Vec4(0.22f, 0.78f, 0.96f, 0.92f), 0.3f, 0.5f, 8.f);
    mb.addCylinder({0.f, deckY + 0.02f, -0.08f}, 1.05f, 1.05f, 0.08f, 20, 1, Vec4(0.25f, 0.65f, 0.92f, 0.85f), 0.3f, 0.5f, 8.f);

    const Vec4 crewCol(0.28f, 0.92f, 0.46f, 0.90f);
    mb.addChamferBox({0.f, 0.82f, -1.55f}, {0.20f, 0.26f, 0.18f}, 0.05f, crewCol, 0.4f, 0.1f, 8.f);
    mb.addEllipsoid({0.f, 1.20f, -1.52f}, {0.12f, 0.14f, 0.12f}, 8, 12, false, crewCol, 0.4f, 0.1f, 8.f);
    for (int side = -1; side <= 1; side += 2) {
        mb.addChamferBox({side * 0.46f, 1.32f, 0.05f}, {0.19f, 0.28f, 0.18f}, 0.05f, crewCol, 0.4f, 0.1f, 8.f);
        mb.addEllipsoid({side * 0.46f, 1.72f, 0.05f}, {0.12f, 0.14f, 0.12f}, 8, 12, false, crewCol, 0.4f, 0.1f, 8.f);
    }
    out.xrayModules = mb.upload(dev);
}

inline void buildEnvironmentAssets(ID3D11Device* dev, EnvironmentGpuAssets& env) {
    env.release();
    MeshBuilder mb;

    // 0. КУПОЛ АТМОСФЕРНОГО НЕБА С СОЛНЦЕМ И ОБЛАКАМИ (matTag = 9)
    {
        mb.clear();
        mb.addEllipsoid({0.f, -25.f, 0.f}, {560.f, 380.f, 560.f}, 24, 36, false, Vec4(1,1,1,1), 1.f, 0.f, 9.f);
        env.skyDome = mb.upload(dev);
    }

    // =========================================================================
    // 1. ОТКРЫТАЯ ВОЕННАЯ БАЗА АНГАРА (С ГОРНЫМ ХРЕБТОМ, ЛЕСОМ, ВЫШКОЙ И ПЛАЦЕМ)
    // =========================================================================
    {
        mb.clear();
        const int G = 64;
        const float span = 210.f;
        const Vec4 grassCol(0.20f, 0.28f, 0.14f, 1.f);
        auto baseH = [](float x, float z) -> float {
            float r = std::hypot(x, z);
            if (r < 27.f) return -0.02f;
            float t = clampf((r - 27.f) / 72.f, 0.f, 1.f);
            float ridge = std::max(0.f, (r - 92.f) * 0.22f) * (0.65f + 0.35f * std::sin(std::atan2(z, x) * 5.f));
            return t * t * (7.5f + 4.8f * std::sin(x * 0.042f) * std::cos(z * 0.039f)) + ridge;
        };
        std::vector<uint32_t> grid((G + 1) * (G + 1));
        for (int gz = 0; gz <= G; ++gz) {
            for (int gx = 0; gx <= G; ++gx) {
                float x = ((float(gx) / G) - 0.5f) * 2.f * span;
                float z = ((float(gz) / G) - 0.5f) * 2.f * span;
                float y = baseH(x, z);
                float hx = baseH(x + 1.4f, z) - baseH(x - 1.4f, z);
                float hz = baseH(x, z + 1.4f) - baseH(x, z - 1.4f);
                Vec3 n = Vec3(-hx, 2.8f, -hz).normalized();
                grid[gz * (G + 1) + gx] = mb.addVert({x, y, z}, n, grassCol, 0.88f, 0.04f, 0.f, 1.f);
            }
        }
        for (int gz = 0; gz < G; ++gz) {
            for (int gx = 0; gx < G; ++gx) {
                uint32_t i0 = grid[gz * (G + 1) + gx];
                uint32_t i1 = i0 + 1;
                uint32_t i2 = i0 + (G + 1);
                uint32_t i3 = i2 + 1;
                mb.addTri(i0, i2, i1);
                mb.addTri(i1, i2, i3);
            }
        }

        // Влажный бетонный плац осмотра техники с разметкой и асфальтовой рулёжной полосой
        mb.addChamferBox({0.f, -0.04f, 0.f}, {23.f, 0.06f, 25.f}, 0.02f, Vec4(0.30f, 0.31f, 0.33f, 1.f), 0.52f, 0.10f, 4.f, 1.f);
        mb.addCylinder({0.f, 0.022f, 0.f}, 5.20f, 5.20f, 0.02f, 48, 1, Vec4(0.24f, 0.25f, 0.28f, 1.f), 0.40f, 0.14f, 4.f, 1.f);

        const Vec4 markYellow(0.84f, 0.62f, 0.12f, 1.f);
        mb.addChamferBox({-5.6f, 0.03f, 0.f}, {0.13f, 0.008f, 5.8f}, 0.f, markYellow, 0.6f, 0.1f, 3.f);
        mb.addChamferBox({ 5.6f, 0.03f, 0.f}, {0.13f, 0.008f, 5.8f}, 0.f, markYellow, 0.6f, 0.1f, 3.f);
        mb.addChamferBox({0.f, 0.03f, -5.8f}, {5.73f, 0.008f, 0.13f}, 0.f, markYellow, 0.6f, 0.1f, 3.f);
        mb.addChamferBox({0.f, 0.03f,  5.8f}, {5.73f, 0.008f, 0.13f}, 0.f, markYellow, 0.6f, 0.1f, 3.f);

        // Ремонтный корпус базы с козырьком, балками, воротами и вентиляцией
        mb.addChamferBox({-13.5f, 3.6f, 18.5f}, {9.8f, 3.6f, 6.2f}, 0.12f, Vec4(0.30f, 0.32f, 0.31f, 1.f), 0.76f, 0.10f, 4.f, 0.90f);
        mb.addChamferBox({-13.5f, 7.35f, 18.0f}, {10.3f, 0.24f, 6.9f}, 0.05f, Vec4(0.20f, 0.22f, 0.24f, 1.f), 0.62f, 0.28f, 3.f);
        for (int d = -1; d <= 1; ++d) {
            mb.addChamferBox({-13.5f + d * 5.8f, 2.55f, 12.25f}, {2.15f, 2.55f, 0.10f}, 0.02f, Vec4(0.16f, 0.20f, 0.18f, 1.f), 0.58f, 0.36f, 3.f, 0.82f);
            mb.addChamferBox({-13.5f + d * 5.8f, 5.45f, 12.18f}, {0.55f, 0.16f, 0.18f}, 0.01f, Vec4(1.0f, 0.86f, 0.58f, 1.f), 0.1f, 0.1f, 6.f);
        }

        // Армейские морские контейнеры, защитные габионы HESCO, топливные цистерны и РЛС-вышка
        mb.addChamferBox({15.8f, 1.32f, 15.2f}, {1.25f, 1.30f, 3.05f}, 0.04f, Vec4(0.18f, 0.26f, 0.17f, 1.f), 0.62f, 0.30f, 3.f, 0.9f);
        mb.addChamferBox({18.6f, 1.32f, 15.2f}, {1.25f, 1.30f, 3.05f}, 0.04f, Vec4(0.32f, 0.26f, 0.18f, 1.f), 0.64f, 0.28f, 3.f, 0.9f);
        for (int h = 0; h < 6; ++h) {
            mb.addChamferBox({6.5f + h * 1.65f, 0.72f, 21.5f}, {0.78f, 0.72f, 0.75f}, 0.04f, Vec4(0.46f, 0.41f, 0.31f, 1.f), 0.88f, 0.04f, 4.f, 0.88f);
        }
        // Радиолокационная вышка ПВО на дальнем плане
        mb.addCylinder({21.f, 6.0f, 22.f}, 0.45f, 0.28f, 12.f, 8, 1, Vec4(0.24f, 0.26f, 0.28f, 1.f), 0.55f, 0.5f, 3.f);
        mb.addChamferBox({21.f, 12.3f, 22.f}, {1.65f, 0.55f, 0.35f}, 0.03f, Vec4(0.28f, 0.32f, 0.26f, 1.f), 0.5f, 0.4f, 3.f);

        for (int m = -1; m <= 1; m += 2) {
            mb.addCylinder({m * 19.5f, 6.0f, -12.f}, 0.18f, 0.11f, 12.f, 10, 1, Vec4(0.24f, 0.26f, 0.28f, 1.f), 0.5f, 0.5f, 3.f);
            mb.addChamferBox({m * 19.5f, 12.0f, -11.7f}, {0.90f, 0.30f, 0.24f}, 0.02f, Vec4(1.0f, 0.94f, 0.80f, 1.f), 0.1f, 0.1f, 6.f);
        }

        // Густой хвойный лес вокруг базы с многоярусными елями
        for (int i = 0; i < 56; ++i) {
            float ang = (float(i) / 56.f) * 2.f * PI;
            float rad = 31.f + (i % 6) * 9.0f;
            float tx = std::cos(ang) * rad;
            float tz = std::sin(ang) * rad;
            if (tz < -8.f && std::fabs(tx) < 14.f) continue;
            float ty = baseH(tx, tz);
            float hScale = 0.82f + (i % 5) * 0.20f;
            addOrganicPineTree(mb, tx, ty, tz, hScale, false);
        }
        env.hangarOutdoor = mb.upload(dev);
    }

    // =========================================================================
    // 2. ЗАКРЫТЫЙ ЖЕЛЕЗОБЕТОННЫЙ АНГАР-БУНКЕР
    // =========================================================================
    {
        mb.clear();
        mb.addChamferBox({0.f, -0.05f, 0.f}, {18.f, 0.05f, 22.f}, 0.f, Vec4(0.22f, 0.23f, 0.25f, 1.f), 0.28f, 0.18f, 4.f);
        mb.addCylinder({0.f, 0.02f, 0.f}, 4.8f, 4.8f, 0.03f, 40, 1, Vec4(0.18f, 0.20f, 0.23f, 1.f), 0.24f, 0.24f, 4.f);
        mb.addChamferBox({0.f, 9.2f, 0.f}, {18.f, 0.25f, 22.f}, 0.f, Vec4(0.16f, 0.17f, 0.19f, 1.f), 0.78f, 0.1f, 4.f);
        mb.addChamferBox({-17.5f, 4.5f, 0.f}, {0.5f, 4.6f, 22.f}, 0.f, Vec4(0.20f, 0.21f, 0.23f, 1.f), 0.75f, 0.1f, 4.f);
        mb.addChamferBox({ 17.5f, 4.5f, 0.f}, {0.5f, 4.6f, 22.f}, 0.f, Vec4(0.20f, 0.21f, 0.23f, 1.f), 0.75f, 0.1f, 4.f);
        mb.addChamferBox({0.f, 4.5f,  21.5f}, {18.f, 4.6f, 0.5f}, 0.f, Vec4(0.18f, 0.19f, 0.21f, 1.f), 0.75f, 0.1f, 4.f);
        mb.addChamferBox({0.f, 4.5f, -21.5f}, {18.f, 4.6f, 0.5f}, 0.f, Vec4(0.18f, 0.19f, 0.21f, 1.f), 0.75f, 0.1f, 4.f);

        for (int z = -16; z <= 16; z += 8) {
            mb.addChamferBox({-16.6f, 4.5f, (float)z}, {0.55f, 4.5f, 0.45f}, 0.04f, Vec4(0.26f, 0.27f, 0.29f, 1.f), 0.7f, 0.15f, 4.f);
            mb.addChamferBox({ 16.6f, 4.5f, (float)z}, {0.55f, 4.5f, 0.45f}, 0.04f, Vec4(0.26f, 0.27f, 0.29f, 1.f), 0.7f, 0.15f, 4.f);
            mb.addChamferBox({0.f, 8.6f, (float)z}, {16.8f, 0.32f, 0.35f}, 0.03f, Vec4(0.24f, 0.25f, 0.28f, 1.f), 0.6f, 0.3f, 3.f);
            mb.addChamferBox({-4.5f, 8.22f, (float)z}, {2.2f, 0.06f, 0.28f}, 0.01f, Vec4(0.95f, 0.97f, 1.0f, 1.f), 0.1f, 0.1f, 6.f);
            mb.addChamferBox({ 4.5f, 8.22f, (float)z}, {2.2f, 0.06f, 0.28f}, 0.01f, Vec4(0.95f, 0.97f, 1.0f, 1.f), 0.1f, 0.1f, 6.f);
        }
        mb.addChamferBox({0.f, 7.6f, 4.f}, {16.4f, 0.35f, 0.45f}, 0.03f, Vec4(0.82f, 0.62f, 0.12f, 1.f), 0.5f, 0.35f, 3.f);
        env.hangarBunker = mb.upload(dev);
    }

    // =========================================================================
    // 3. БОЕВЫЕ КАРТЫ И ПОЛИГОН
    // =========================================================================
    const auto& maps = getMaps();
    for (int m = 0; m < 4 && m < (int)maps.size(); ++m) {
        mb.clear();
        const MapSpec& ms = maps[m];
        const int N = 120;
        const float half = ms.size * 0.5f;
        Vec4 gCol(ms.groundColor.x, ms.groundColor.y, ms.groundColor.z, 1.f);

        std::vector<uint32_t> grid((N + 1) * (N + 1));
        for (int gz = 0; gz <= N; ++gz) {
            for (int gx = 0; gx <= N; ++gx) {
                float x = ((float(gx) / N) - 0.5f) * 2.f * half;
                float z = ((float(gz) / N) - 0.5f) * 2.f * half;
                float y = getTerrainHeight(m, x, z);
                float dx = getTerrainHeight(m, x + 1.4f, z) - getTerrainHeight(m, x - 1.4f, z);
                float dz = getTerrainHeight(m, x, z + 1.4f) - getTerrainHeight(m, x, z - 1.4f);
                Vec3 n = Vec3(-dx, 2.8f, -dz).normalized();

                bool onRoad = (std::fabs(x) < 5.2f || (!ms.isPoligon && std::fabs(z) < 5.2f));
                Vec4 vCol = onRoad ? Vec4(0.22f, 0.22f, 0.21f, 1.f) : gCol;
                grid[gz * (N + 1) + gx] = mb.addVert({x, y, z}, n, vCol, onRoad ? 0.68f : 0.88f, 0.04f, onRoad ? 4.f : 0.f, 1.f);
            }
        }
        for (int gz = 0; gz < N; ++gz) {
            for (int gx = 0; gx < N; ++gx) {
                uint32_t i0 = grid[gz * (N + 1) + gx];
                uint32_t i1 = i0 + 1;
                uint32_t i2 = i0 + (N + 1);
                uint32_t i3 = i2 + 1;
                mb.addTri(i0, i2, i1);
                mb.addTri(i1, i2, i3);
            }
        }

        for (const auto& ob : ms.obstacles) {
            float by = getTerrainHeight(m, ob.x, ob.z);
            Mat4 obXf = Mat4::rotationY(ob.rot) * Mat4::translation(ob.x, by, ob.z);
            if (ob.kind == OB_ROCK) {
                mb.addEllipsoid({ob.x, by + ob.h * 0.35f, ob.z}, {ob.w * 0.55f, ob.h * 0.65f, ob.d * 0.55f},
                                9, 12, false, Vec4(0.32f, 0.31f, 0.30f, 1.f), 0.85f, 0.05f, 4.f, 0.88f);
            } else if (ob.kind == OB_BUNKER || ob.kind == OB_WALL) {
                mb.addChamferBox({0.f, ob.h * 0.5f - 0.3f, 0.f}, {ob.w * 0.5f, ob.h * 0.5f + 0.3f, ob.d * 0.5f},
                                 0.16f, Vec4(0.38f, 0.39f, 0.37f, 1.f), 0.80f, 0.08f, 4.f, 0.90f, &obXf);
            } else if (ob.kind == OB_SILO) {
                mb.addCylinder({ob.x, by + ob.h * 0.5f, ob.z}, ob.w * 0.48f, ob.w * 0.46f, ob.h, 20, 1,
                               Vec4(0.50f, 0.52f, 0.54f, 1.f), 0.48f, 0.38f, 3.f, 0.94f);
            } else {
                mb.addChamferBox({0.f, ob.h * 0.5f - 0.4f, 0.f}, {ob.w * 0.5f, ob.h * 0.5f + 0.4f, ob.d * 0.5f},
                                 0.12f, Vec4(0.44f, 0.40f, 0.36f, 1.f), 0.78f, 0.08f, 4.f, 0.92f, &obXf);
                mb.addChamferBox({0.f, ob.h + 0.18f, 0.f}, {ob.w * 0.52f, 0.22f, ob.d * 0.52f},
                                 0.06f, Vec4(0.28f, 0.20f, 0.18f, 1.f), 0.68f, 0.20f, 3.f, 0.95f, &obXf);
            }
        }

        uint32_t tSeed = 1337u + m * 7919u;
        for (int t = 0; t < 72; ++t) {
            tSeed = tSeed * 1664525u + 1013904223u;
            float tx = ((float((tSeed >> 8) & 0xFFFF) / 65535.f) - 0.5f) * 1.82f * half;
            tSeed = tSeed * 1664525u + 1013904223u;
            float tz = ((float((tSeed >> 8) & 0xFFFF) / 65535.f) - 0.5f) * 1.82f * half;
            if (std::fabs(tx) < 18.f || std::fabs(tz) < 18.f) continue;
            bool nearZone = false;
            for (const auto& zn : ms.zones) if (std::hypot(tx - zn.x, tz - zn.z) < zn.radius + 8.f) nearZone = true;
            if (nearZone) continue;

            float ty = getTerrainHeight(m, tx, tz);
            float sc = 0.85f + (t % 4) * 0.22f;
            addOrganicPineTree(mb, tx, ty, tz, sc, m == 2);
        }

        if (ms.isPoligon) {
            for (int r = 1; r <= 4; ++r) {
                float rz = 128.f - r * 70.f;
                float ry = getTerrainHeight(m, -36.f, rz);
                mb.addChamferBox({-36.f, ry + 1.4f, rz}, {2.2f, 1.1f, 0.12f}, 0.02f, Vec4(0.85f, 0.65f, 0.14f, 1.f), 0.5f, 0.2f, 3.f);
            }
        }
        env.mapTerrain[m] = mb.upload(dev);
    }

    // 4. Меши спецэффектов
    mb.clear();
    mb.addEllipsoid({0,0,0}, {1,1,1}, 12, 16, false, Vec4(1,1,1,1), 0.8f, 0.0f, 6.f);
    env.unitSphere = mb.upload(dev);

    mb.clear();
    mb.addCylinder({0,0,0}, 0.08f, 0.04f, 3.4f, 8, 2, Vec4(1,1,1,1), 0.1f, 0.0f, 6.f);
    env.unitTracer = mb.upload(dev);

    mb.clear();
    const int RS = 48;
    for (int i = 0; i < RS; ++i) {
        float a0 = (float(i) / RS) * 2.f * PI;
        float a1 = (float(i + 1) / RS) * 2.f * PI;
        if (i % 2 == 0) {
            Vec3 p0(std::cos(a0) * 0.96f, 0.18f, std::sin(a0) * 0.96f);
            Vec3 p1(std::cos(a0) * 1.00f, 0.18f, std::sin(a0) * 1.00f);
            Vec3 p2(std::cos(a1) * 1.00f, 0.18f, std::sin(a1) * 1.00f);
            Vec3 p3(std::cos(a1) * 0.96f, 0.18f, std::sin(a1) * 0.96f);
            mb.addQuad(p0, p3, p2, p1, Vec4(1,1,1,0.92f), 0.2f, 0.0f, 6.f);
        }
    }
    env.zoneRing = mb.upload(dev);
}

} // namespace sf
