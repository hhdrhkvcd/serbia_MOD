#pragma once
#include "d3d11_renderer.h"
#include "game_data.h"

namespace sf {

struct VehicleGpuModel {
    GpuMesh hull;
    GpuMesh turret;
    GpuMesh gun;         // Начало координат в цапфах орудия (0, 0, 0), смещение от башни: (0, 1.72, -0.42)
    GpuMesh xrayModules; // Двигатель, трансмиссия, карусель/ниша БК, казённик, погон, экипаж

    void release() {
        hull.release();
        turret.release();
        gun.release();
        xrayModules.release();
    }
};

struct EnvironmentGpuAssets {
    GpuMesh hangarOutdoor;
    GpuMesh hangarBunker;
    GpuMesh mapTerrain[4];
    GpuMesh unitSphere;
    GpuMesh unitTracer;
    GpuMesh zoneRing;

    void release() {
        hangarOutdoor.release();
        hangarBunker.release();
        for (auto& m : mapTerrain) m.release();
        unitSphere.release();
        unitTracer.release();
        zoneRing.release();
    }
};

inline void buildVehicleMeshes(ID3D11Device* dev, const VehicleSpec& v, VehicleGpuModel& out) {
    out.release();
    const Vec4 armorBase(1.0f, 1.0f, 1.0f, 1.0f);       // Окрашивается через CBObject tint/camo (tag=1)
    const Vec4 armorDark(0.76f, 0.78f, 0.75f, 1.0f);    // Притемнённые ниши/решётки (tag=1)
    const Vec4 eraCol(0.88f, 0.90f, 0.86f, 1.0f);       // Блоки ДЗ Контакт-5/Реликт/M-19 (tag=7)
    const Vec4 rubberCol(0.11f, 0.11f, 0.12f, 1.0f);    // Бандажи и резина (tag=2)
    const Vec4 trackCol(0.16f, 0.15f, 0.14f, 1.0f);     // Стальные траки (tag=2)
    const Vec4 steelCol(0.24f, 0.25f, 0.27f, 1.0f);     // Оружейная сталь / пулемёты (tag=3)
    const Vec4 grilleCol(0.07f, 0.07f, 0.08f, 1.0f);    // Жалюзи МТО (tag=3)
    const Vec4 opticCol(0.12f, 0.58f, 0.86f, 1.0f);     // Просветлённая оптика прицелов (tag=6)
    const Vec4 headLight(0.96f, 0.92f, 0.74f, 1.0f);    // Фары (tag=6)

    const bool isEast = (v.arch == ARCH_TSERIES);
    const bool isWest = (v.arch == ARCH_WESTERN || v.arch == ARCH_CHINESE);
    const bool isIfv  = (v.arch == ARCH_IFV && std::strcmp(v.id, "m10") != 0);
    const bool isWw2  = (std::strcmp(v.id, "m10") == 0);
    const bool hasEra = (v.eraName && v.eraName[0] != 0 && std::strcmp(v.eraName, "—") != 0 && std::strcmp(v.eraName, "Нет") != 0);
    const bool hasHeavySkirts = (isEast || isWest) && !isWw2;

    const float hullHalfW = isIfv ? 1.12f : 1.24f;
    const float hullLenF  = isIfv ? -3.15f : (isWest ? -3.55f : -3.35f);
    const float hullLenR  = isIfv ?  3.05f : (isWest ?  3.50f :  3.28f);
    const float deckY     = isIfv ?  1.34f : (isWw2  ?  1.42f :  1.26f);

    // =========================================================================
    // 1. КОРПУС + ХОДОВАЯ ЧАСТЬ (КАТКИ, ГУСЕНИЦЫ, ВЛД/НЛД, ДЗ, МТО, БОЧКИ)
    // =========================================================================
    MeshBuilder mb;

    // Основная ванна корпуса со скошенными ВЛД и НЛД (8-вершинная бронепризма)
    Vec3 hullLower[8] = {
        {-hullHalfW, 0.42f, hullLenF + 0.58f}, { hullHalfW, 0.42f, hullLenF + 0.58f},
        { hullHalfW, 0.44f, hullLenR - 0.35f}, {-hullHalfW, 0.44f, hullLenR - 0.35f},
        {-hullHalfW, 0.86f, hullLenF},         { hullHalfW, 0.86f, hullLenF},
        { hullHalfW, 0.90f, hullLenR},         {-hullHalfW, 0.90f, hullLenR}
    };
    mb.addPrism8(hullLower, armorBase, 0.54f, 0.22f, 1.f, 0.85f);

    Vec3 hullUpper[8] = {
        {-hullHalfW,        0.86f, hullLenF},         { hullHalfW,        0.86f, hullLenF},
        { hullHalfW,        0.90f, hullLenR},         {-hullHalfW,        0.90f, hullLenR},
        {-hullHalfW + 0.08f, deckY, hullLenF + 1.35f},{ hullHalfW - 0.08f, deckY, hullLenF + 1.35f},
        { hullHalfW - 0.06f, deckY + 0.05f, hullLenR - 0.18f}, {-hullHalfW + 0.06f, deckY + 0.05f, hullLenR - 0.18f}
    };
    mb.addPrism8(hullUpper, armorBase, 0.50f, 0.25f, 1.f, 1.0f);

    // Надгусеничные полки и скошенные бортовые экраны
    for (int side = -1; side <= 1; side += 2) {
        float sx = side * (hullHalfW + 0.30f);
        mb.addChamferBox({sx, 0.98f, 0.0f}, {0.26f, 0.08f, (hullLenR - hullLenF) * 0.49f}, 0.03f,
                         armorBase, 0.52f, 0.24f, 1.f, 0.95f);

        // Бортовые противокумулятивные / композитные экраны
        int skirtPanels = isWest ? 5 : 6;
        float zSpan = (hullLenR - hullLenF) * 0.88f;
        float panelLen = zSpan / skirtPanels;
        for (int p = 0; p < skirtPanels; ++p) {
            float pz = hullLenF + 0.42f + (p + 0.5f) * panelLen;
            bool heavyFrontSkirt = hasHeavySkirts && (p < 3);
            mb.addChamferBox({side * (hullHalfW + 0.53f), heavyFrontSkirt ? 0.80f : 0.77f, pz},
                             {heavyFrontSkirt ? 0.065f : 0.035f, heavyFrontSkirt ? 0.25f : 0.20f, panelLen * 0.47f},
                             0.02f, heavyFrontSkirt ? eraCol : armorDark, 0.56f, 0.20f,
                             heavyFrontSkirt ? 7.f : 1.f, 0.92f);
        }

        // Гусеничная лента (верхняя, нижняя и наклонные ветви) + катки + ведущее/направляющее колёса
        float tx = side * (hullHalfW + 0.28f);
        int numWheels = isWest ? 7 : 6;
        float zFirstW = hullLenF + 0.85f;
        float zLastW  = hullLenR - 0.85f;

        // Нижняя и верхняя ветви траков
        mb.addChamferBox({tx, 0.07f, (zFirstW + zLastW) * 0.5f}, {0.26f, 0.055f, (zLastW - zFirstW) * 0.52f}, 0.015f,
                         trackCol, 0.78f, 0.38f, 2.f, 0.65f);
        mb.addChamferBox({tx, 0.84f, 0.0f}, {0.26f, 0.045f, (hullLenR - hullLenF) * 0.45f}, 0.015f,
                         trackCol, 0.78f, 0.38f, 2.f, 0.75f);

        // Наклонные ветви гусеницы к ленивцу и ведущей звёздочке
        Mat4 frontSlope = Mat4::translation(tx, 0.34f, hullLenF + 0.46f) * Mat4::rotationX(-0.62f);
        mb.addChamferBox({0,0,0}, {0.26f, 0.05f, 0.46f}, 0.01f, trackCol, 0.78f, 0.38f, 2.f, 0.75f, &frontSlope);
        Mat4 rearSlope  = Mat4::translation(tx, 0.36f, hullLenR - 0.46f) * Mat4::rotationX(0.62f);
        mb.addChamferBox({0,0,0}, {0.26f, 0.05f, 0.46f}, 0.01f, trackCol, 0.78f, 0.38f, 2.f, 0.75f, &rearSlope);

        // Опорные катки (резиновый бандаж + штампованный диск + ступица)
        for (int w = 0; w < numWheels; ++w) {
            float wz = zFirstW + (float(w) / (numWheels - 1)) * (zLastW - zFirstW);
            float rWheel = isIfv ? 0.31f : 0.36f;
            mb.addCylinder({tx, rWheel + 0.05f, wz}, rWheel, rWheel, 0.44f, 18, 0,
                           rubberCol, 0.86f, 0.08f, 2.f, 0.72f);
            mb.addCylinder({tx + side * 0.03f, rWheel + 0.05f, wz}, rWheel * 0.74f, rWheel * 0.68f, 0.46f, 16, 0,
                           armorBase, 0.52f, 0.28f, 1.f, 0.82f);
            mb.addCylinder({tx + side * 0.14f, rWheel + 0.05f, wz}, 0.11f, 0.09f, 0.26f, 12, 0,
                           steelCol, 0.44f, 0.55f, 3.f, 0.90f);
        }
        // Направляющее колесо (ленивец) спереди и ведущее зубчатое колесо сзади
        mb.addCylinder({tx, 0.56f, hullLenF + 0.30f}, 0.29f, 0.29f, 0.42f, 16, 0, steelCol, 0.48f, 0.45f, 3.f, 0.8f);
        mb.addCylinder({tx, 0.58f, hullLenR - 0.30f}, 0.31f, 0.31f, 0.44f, 16, 0, steelCol, 0.45f, 0.52f, 3.f, 0.8f);
    }

    // Блоки динамической защиты (Контакт-5 / Реликт / M-19) на ВЛД
    if (hasEra) {
        Mat4 glacisXf = Mat4::translation(0.f, 1.07f, hullLenF + 0.72f) * Mat4::rotationX(0.32f);
        for (int row = 0; row < 2; ++row) {
            for (int col = -3; col <= 3; ++col) {
                if (row == 1 && col == 0) continue; // вырез под триплекс механика-водителя
                Vec3 localOff(col * 0.32f, 0.035f, (row - 0.5f) * 0.34f);
                Mat4 tileXf = Mat4::translation(localOff.x, localOff.y, localOff.z) * glacisXf;
                mb.addChamferBox({0,0,0}, {0.145f, 0.04f, 0.155f}, 0.015f, eraCol, 0.48f, 0.28f, 7.f, 0.96f, &tileXf);
            }
        }
    }

    // Перископ механика-водителя и фары с ограждением
    mb.addChamferBox({0.0f, deckY + 0.05f, hullLenF + 1.38f}, {0.22f, 0.06f, 0.14f}, 0.02f, steelCol, 0.4f, 0.5f, 3.f);
    mb.addChamferBox({0.0f, deckY + 0.07f, hullLenF + 1.25f}, {0.14f, 0.025f, 0.03f}, 0.005f, opticCol, 0.1f, 0.9f, 6.f);
    for (int side = -1; side <= 1; side += 2) {
        mb.addCylinder({side * 1.18f, 1.06f, hullLenF + 0.36f}, 0.085f, 0.075f, 0.12f, 12, 2, steelCol, 0.45f, 0.5f, 3.f);
        mb.addCylinder({side * 1.18f, 1.06f, hullLenF + 0.29f}, 0.065f, 0.065f, 0.02f, 12, 2, headLight, 0.1f, 0.1f, 6.f);
    }

    // Крыша МТО с радиаторными решётками и выхлопом
    mb.addChamferBox({0.f, deckY + 0.07f, hullLenR - 1.05f}, {1.06f, 0.055f, 0.85f}, 0.03f, armorDark, 0.56f, 0.25f, 1.f);
    for (int g = -1; g <= 1; g += 2) {
        mb.addChamferBox({g * 0.52f, deckY + 0.125f, hullLenR - 1.12f}, {0.38f, 0.018f, 0.52f}, 0.005f, grilleCol, 0.88f, 0.35f, 3.f, 0.55f);
    }

    // Навесные кормовые топливные бочки и бревно самовытаскивания (Т-72Б3, Т-80, Т-90М, M-84AS1/AS2, ZTZ-96/99)
    if (v.hasFuelDrums) {
        for (int side = -1; side <= 1; side += 2) {
            mb.addCylinder({side * 0.68f, 1.18f, hullLenR + 0.22f}, 0.24f, 0.24f, 0.92f, 16, 0,
                           armorDark, 0.55f, 0.25f, 1.f, 0.9f);
            mb.addChamferBox({side * 0.68f, 1.02f, hullLenR + 0.05f}, {0.35f, 0.04f, 0.16f}, 0.01f, steelCol, 0.5f, 0.5f, 3.f);
        }
    }
    if (v.hasLog) {
        mb.addCylinder({0.f, 0.96f, hullLenR + 0.12f}, 0.09f, 0.085f, 2.55f, 12, 0,
                       Vec4(0.38f, 0.27f, 0.16f, 1.f), 0.88f, 0.02f, 5.f, 0.9f);
    }
    // Противокумулятивные кормовые решётчатые экраны (Т-90М, M-84AS2 Војвода, Т-80БВМ)
    if (isEast && v.hasRcws) {
        for (int bar = 0; bar < 4; ++bar) {
            mb.addChamferBox({0.f, 0.68f + bar * 0.13f, hullLenR + 0.10f}, {1.38f, 0.015f, 0.03f}, 0.004f, steelCol, 0.5f, 0.5f, 3.f);
        }
    }
    out.hull = mb.upload(dev);

    // =========================================================================
    // 2. БАШНЯ (КУПОЛ/КЛИНЬЯ, СКУЛЫ ДЗ, ПАНОРАМНЫЙ ПРИЦЕЛ, ТУЧА, НСВТ/ДУБМ, НИША)
    // =========================================================================
    mb.clear();
    const float turBaseY = deckY + 0.04f;

    if (isEast) {
        // Скульптурный литой/сварной купол башни серии Т-72 / Т-80 / Т-90М / M-84AS2 «Војвода»
        mb.addCylinder({0.f, turBaseY + 0.10f, -0.08f}, 1.22f, 1.18f, 0.20f, 24, 1, armorBase, 0.50f, 0.26f, 1.f, 0.9f);
        mb.addEllipsoid({0.f, turBaseY + 0.14f, -0.08f}, {1.30f, 0.56f, 1.38f}, 14, 24, true, armorBase, 0.48f, 0.26f, 1.f, 1.0f);

        // Задняя сварная кормовая ниша («Прорыв» Т-90М / M-84AS2 «Војвода» / M-91 «Вихор»)
        if (v.hasBustle) {
            mb.addChamferBox({0.f, turBaseY + 0.36f, 1.18f}, {0.92f, 0.26f, 0.48f}, 0.05f, armorBase, 0.50f, 0.25f, 1.f, 0.95f);
        }

        // Клиновидные модули ДЗ «Контакт-5» / «Реликт» / «M-19» по дуге скул башни!
        if (hasEra) {
            for (int side = -1; side <= 1; side += 2) {
                for (int k = 0; k < 4; ++k) {
                    float ang = side * (0.30f + k * 0.26f);
                    float rx = std::sin(ang) * 1.30f;
                    float rz = -0.08f - std::cos(ang) * 1.34f;
                    Mat4 wXf = Mat4::rotationY(-ang) * Mat4::translation(rx, turBaseY + 0.32f, rz);
                    Mat4 upW = Mat4::rotationX(0.45f) * wXf;
                    Mat4 dnW = Mat4::rotationX(-0.42f) * Mat4::translation(0.f, -0.14f, 0.f) * wXf;
                    mb.addChamferBox({0,0,0}, {0.17f, 0.13f, 0.11f}, 0.02f, eraCol, 0.46f, 0.30f, 7.f, 0.98f, &upW);
                    mb.addChamferBox({0,0,0}, {0.17f, 0.11f, 0.10f}, 0.02f, eraCol, 0.46f, 0.30f, 7.f, 0.92f, &dnW);
                }
            }
        }
    } else if (isWest) {
        // Многогранная клиновидная западная/китайская башня (M1A2, Leo 2A7V, Strv 122, Leclerc, ZTZ-99A)
        Vec3 turCore[8] = {
            {-1.28f, turBaseY,        -1.15f}, { 1.28f, turBaseY,        -1.15f},
            { 1.24f, turBaseY,         1.85f}, {-1.24f, turBaseY,         1.85f},
            {-1.18f, turBaseY + 0.68f,-1.02f}, { 1.18f, turBaseY + 0.68f,-1.02f},
            { 1.15f, turBaseY + 0.68f, 1.78f}, {-1.15f, turBaseY + 0.68f, 1.78f}
        };
        mb.addPrism8(turCore, armorBase, 0.48f, 0.25f, 1.f, 1.0f);

        // Острые стреловидные накладные скулы («клюв» Leopard 2A7 / Strv 122 / ZTZ-99A / M1A2)
        if (v.wedgeTurret) {
            for (int side = -1; side <= 1; side += 2) {
                float sx0 = side * 0.32f, sx1 = side * 1.28f;
                Vec3 cheek[8] = {
                    {std::min(sx0, sx1), turBaseY + 0.05f, -1.78f}, {std::max(sx0, sx1), turBaseY + 0.05f, -1.78f},
                    {std::max(sx0, sx1), turBaseY + 0.02f, -1.05f}, {std::min(sx0, sx1), turBaseY + 0.02f, -1.05f},
                    {std::min(sx0, sx1), turBaseY + 0.64f, -1.52f}, {std::max(sx0, sx1), turBaseY + 0.64f, -1.26f},
                    {std::max(sx0, sx1), turBaseY + 0.68f, -1.02f}, {std::min(sx0, sx1), turBaseY + 0.68f, -1.02f}
                };
                if (side < 0) { cheek[4].z = -1.26f; cheek[5].z = -1.52f; }
                mb.addPrism8(cheek, hasEra ? eraCol : armorBase, 0.46f, 0.26f, hasEra ? 7.f : 1.f, 0.98f);
            }
        }
        // Вышибные панели БК на крыше кормовой ниши
        mb.addChamferBox({-0.45f, turBaseY + 0.70f, 1.15f}, {0.38f, 0.025f, 0.48f}, 0.01f, armorDark, 0.55f, 0.24f, 1.f);
        mb.addChamferBox({ 0.45f, turBaseY + 0.70f, 1.15f}, {0.38f, 0.025f, 0.48f}, 0.01f, armorDark, 0.55f, 0.24f, 1.f);
    } else if (isIfv) {
        // Компактная скошенная башня БМП BVP M-80AB1 / CV 90120
        Vec3 ifvTur[8] = {
            {-0.88f, turBaseY,        -0.88f}, { 0.88f, turBaseY,        -0.88f},
            { 0.84f, turBaseY,         0.92f}, {-0.84f, turBaseY,         0.92f},
            {-0.68f, turBaseY + 0.50f,-0.65f}, { 0.68f, turBaseY + 0.50f,-0.65f},
            { 0.68f, turBaseY + 0.52f, 0.82f}, {-0.68f, turBaseY + 0.52f, 0.82f}
        };
        mb.addPrism8(ifvTur, armorBase, 0.50f, 0.24f, 1.f, 1.0f);

        // Спаренная пусковая установка ПТУР «Малютка-2Т5» на сербской BVP M-80AB1!
        if (v.hasAtgmRails) {
            mb.addChamferBox({0.62f, turBaseY + 0.62f, 0.15f}, {0.26f, 0.06f, 0.22f}, 0.015f, steelCol, 0.45f, 0.5f, 3.f);
            for (int r = -1; r <= 1; r += 2) {
                mb.addCylinder({0.62f + r * 0.14f, turBaseY + 0.76f, 0.10f}, 0.075f, 0.065f, 0.86f, 12, 2,
                               eraCol, 0.45f, 0.30f, 7.f, 0.95f);
                mb.addCylinder({0.62f + r * 0.14f, turBaseY + 0.76f, -0.38f}, 0.01f, 0.065f, 0.16f, 12, 2,
                               Vec4(0.75f, 0.20f, 0.16f, 1.f), 0.4f, 0.3f, 3.f, 1.0f);
            }
        }
    } else {
        // Пятигранная американская башня САУ M10 Wolverine
        Vec3 tdTur[8] = {
            {-1.05f, turBaseY,        -0.95f}, { 1.05f, turBaseY,        -0.95f},
            { 0.98f, turBaseY,         1.35f}, {-0.98f, turBaseY,         1.35f},
            {-0.88f, turBaseY + 0.66f,-0.80f}, { 0.88f, turBaseY + 0.66f,-0.80f},
            { 0.86f, turBaseY + 0.66f, 1.48f}, {-0.86f, turBaseY + 0.66f, 1.48f}
        };
        mb.addPrism8(tdTur, armorBase, 0.54f, 0.22f, 1.f, 1.0f);
    }

    // Командирская башенка, прицел наводчика «Сосна-У»/GPS, панорамный CITV и пулемёт НСВТ/M2HB
    float roofY = turBaseY + (isIfv ? 0.52f : (isEast ? 0.56f : 0.68f));
    mb.addCylinder({0.48f, roofY + 0.08f, 0.18f}, 0.30f, 0.27f, 0.16f, 16, 1, armorDark, 0.5f, 0.28f, 1.f);
    // Прицел наводчика с голубой просветлённой линзой
    mb.addChamferBox({-0.52f, roofY + 0.13f, -0.35f}, {0.16f, 0.13f, 0.20f}, 0.02f, armorDark, 0.48f, 0.3f, 1.f);
    mb.addChamferBox({-0.52f, roofY + 0.14f, -0.56f}, {0.11f, 0.08f, 0.015f}, 0.004f, opticCol, 0.1f, 0.9f, 6.f);

    if (v.hasRcws || isWest) {
        mb.addCylinder({0.42f, roofY + 0.24f, 0.48f}, 0.17f, 0.15f, 0.28f, 14, 1, armorDark, 0.48f, 0.3f, 1.f);
        mb.addChamferBox({0.42f, roofY + 0.27f, 0.33f}, {0.10f, 0.07f, 0.02f}, 0.004f, opticCol, 0.1f, 0.9f, 6.f);
    }

    // Зенитный крупнокалиберный пулемёт 12.7-мм (НСВТ / Корд / M-15 ДУБМ / M2HB)
    mb.addChamferBox({0.48f, roofY + 0.26f, 0.12f}, {0.11f, 0.10f, 0.22f}, 0.015f, steelCol, 0.42f, 0.58f, 3.f);
    mb.addCylinder({0.48f, roofY + 0.30f, -0.45f}, 0.024f, 0.018f, 0.95f, 10, 2, steelCol, 0.38f, 0.65f, 3.f);

    // Дымовые гранатомёты 902Б «Туча» / Wegmann по скулам башни (по 4 мортирки с каждого борта)
    for (int side = -1; side <= 1; side += 2) {
        for (int s = 0; s < 4; ++s) {
            Mat4 smXf = Mat4::rotationX(-0.42f) * Mat4::rotationY(side * -0.35f) *
                        Mat4::translation(side * (isIfv ? 0.88f : 1.24f), turBaseY + 0.36f + (s % 2) * 0.10f, -0.15f + (s / 2) * 0.14f);
            mb.addCylinder({0,0,0}, 0.042f, 0.042f, 0.24f, 10, 2, steelCol, 0.45f, 0.5f, 3.f, 0.9f, true, &smXf);
        }
    }
    // Гибкие радиоантенны
    mb.addCylinder({-0.78f, roofY + 0.85f, 0.75f}, 0.010f, 0.005f, 1.70f, 6, 1, steelCol, 0.4f, 0.6f, 3.f);

    out.turret = mb.upload(dev);

    // =========================================================================
    // 3. ОРУДИЕ И МАСКА (ВРАЩАЕТСЯ ПО УГЛУ ВОЗВЫШЕНИЯ ВОКРУГ ЦАПФ 0, 1.72, -0.45)
    // =========================================================================
    mb.clear();
    // Бронированная маска орудия + спаренный 7.62-мм ПКТ
    mb.addChamferBox({0.f, 0.f, -0.42f}, {0.32f, 0.22f, 0.34f}, 0.04f, armorDark, 0.48f, 0.30f, 1.f, 0.95f);
    mb.addCylinder({0.20f, 0.02f, -0.88f}, 0.018f, 0.016f, 0.52f, 8, 2, steelCol, 0.38f, 0.65f, 3.f);

    float gunR  = isIfv ? 0.048f : (isWw2 ? 0.068f : 0.088f);
    float gunLen = isIfv ? 2.45f  : (isWest ? 4.55f : 4.28f);

    // Корневой кожух ствола
    mb.addCylinder({0.f, 0.f, -1.25f}, gunR * 1.22f, gunR * 1.08f, 1.35f, 18, 2, armorBase, 0.48f, 0.30f, 1.f);
    // Эжектор пороховых газов (утолщение посередине ствола 2А46М / Rh-120 / M256)
    if (!isIfv && !isWw2) {
        mb.addCylinder({0.f, 0.018f, -2.28f}, gunR * 1.56f, gunR * 1.52f, 0.68f, 20, 2, armorDark, 0.46f, 0.34f, 1.f);
    }
    // Основная труба ствола с термозащитным кожухом
    mb.addCylinder({0.f, 0.f, -0.70f - gunLen * 0.5f}, gunR, gunR * 0.92f, gunLen, 18, 2, armorBase, 0.46f, 0.34f, 1.f);
    // Дульное кольцо и датчик изгиба ствола (УУИ)
    mb.addCylinder({0.f, 0.f, -0.70f - gunLen - 0.06f}, gunR * 1.12f, gunR * 1.08f, 0.14f, 18, 2, steelCol, 0.36f, 0.68f, 3.f);
    mb.addCylinder({0.f, 0.f, -0.70f - gunLen - 0.135f}, gunR * 0.68f, gunR * 0.68f, 0.02f, 16, 2, Vec4(0.02f,0.02f,0.02f,1.f), 0.9f, 0.1f, 3.f);
    if (!isIfv && !isWw2) {
        mb.addChamferBox({0.06f, gunR * 1.28f, -0.70f - gunLen - 0.04f}, {0.025f, 0.035f, 0.05f}, 0.005f, steelCol, 0.4f, 0.6f, 3.f);
    }
    out.gun = mb.upload(dev);

    // =========================================================================
    // 4. ВНУТРЕННИЕ МОДУЛИ X-RAY (ДЛЯ РЕНТГЕНА В АНГАРЕ И 3D КАМЕРЫ ПОПАДАНИЙ!)
    // =========================================================================
    mb.clear();
    // Двигатель V-12 / ГТД и бортовые коробки передач (янтарно-оранжевый контур)
    mb.addChamferBox({0.f, 0.76f, 1.75f}, {0.68f, 0.34f, 0.82f}, 0.05f, Vec4(0.95f, 0.48f, 0.12f, 0.92f), 0.3f, 0.5f, 8.f);
    mb.addCylinder({0.f, 0.72f, 2.65f}, 0.36f, 0.36f, 0.78f, 14, 0, Vec4(0.85f, 0.38f, 0.14f, 0.92f), 0.3f, 0.5f, 8.f);

    // Боеукладка: карусель АЗ на днище (восточная школа) или кормовая ниша башни (западная школа) — ало-золотой
    if (isEast) {
        mb.addCylinder({0.f, 0.52f, -0.08f}, 0.92f, 0.92f, 0.28f, 20, 1, Vec4(0.96f, 0.22f, 0.18f, 0.94f), 0.3f, 0.5f, 8.f);
    } else {
        mb.addChamferBox({0.f, turBaseY + 0.34f, 1.18f}, {0.78f, 0.22f, 0.42f}, 0.03f, Vec4(0.96f, 0.22f, 0.18f, 0.94f), 0.3f, 0.5f, 8.f);
    }

    // Казённик орудия и привод погона башни (бирюзово-голубой)
    mb.addChamferBox({0.f, 1.66f, -0.15f}, {0.24f, 0.20f, 0.55f}, 0.03f, Vec4(0.22f, 0.78f, 0.96f, 0.92f), 0.3f, 0.5f, 8.f);
    mb.addCylinder({0.f, deckY + 0.02f, -0.08f}, 1.05f, 1.05f, 0.08f, 20, 1, Vec4(0.25f, 0.65f, 0.92f, 0.85f), 0.3f, 0.5f, 8.f);

    // Экипаж (механик-водитель, наводчик, командир — изумрудно-зелёные силуэты)
    const Vec4 crewCol(0.28f, 0.92f, 0.46f, 0.90f);
    // Мехвод
    mb.addChamferBox({0.f, 0.82f, -1.55f}, {0.20f, 0.26f, 0.18f}, 0.05f, crewCol, 0.4f, 0.1f, 8.f);
    mb.addEllipsoid({0.f, 1.20f, -1.52f}, {0.12f, 0.14f, 0.12f}, 8, 12, false, crewCol, 0.4f, 0.1f, 8.f);
    // Наводчик и командир
    for (int side = -1; side <= 1; side += 2) {
        mb.addChamferBox({side * 0.46f, 1.32f, 0.05f}, {0.19f, 0.28f, 0.18f}, 0.05f, crewCol, 0.4f, 0.1f, 8.f);
        mb.addEllipsoid({side * 0.46f, 1.72f, 0.05f}, {0.12f, 0.14f, 0.12f}, 8, 12, false, crewCol, 0.4f, 0.1f, 8.f);
    }
    out.xrayModules = mb.upload(dev);
}

// Построение открытой военной базы и закрытого бункера ангара, плюс боевых карт и спецэффектов
inline void buildEnvironmentAssets(ID3D11Device* dev, EnvironmentGpuAssets& env) {
    env.release();
    MeshBuilder mb;

    // =========================================================================
    // 1. ОТКРЫТАЯ ВОЕННАЯ БАЗА АНГАРА (КАК В WAR THUNDER!)
    // =========================================================================
    {
        mb.clear();
        // Окружающий зелёный полигон с холмами на горизонте
        const int G = 48;
        const float span = 180.f;
        const Vec4 grassCol(0.24f, 0.33f, 0.18f, 1.f);
        auto baseH = [](float x, float z) -> float {
            float r = std::hypot(x, z);
            if (r < 28.f) return -0.02f;
            float t = clampf((r - 28.f) / 60.f, 0.f, 1.f);
            return t * t * (9.5f + 5.5f * std::sin(x * 0.045f) * std::cos(z * 0.042f));
        };
        std::vector<uint32_t> grid((G + 1) * (G + 1));
        for (int gz = 0; gz <= G; ++gz) {
            for (int gx = 0; gx <= G; ++gx) {
                float x = ((float(gx) / G) - 0.5f) * 2.f * span;
                float z = ((float(gz) / G) - 0.5f) * 2.f * span;
                float y = baseH(x, z);
                float hx = baseH(x + 1.2f, z) - baseH(x - 1.2f, z);
                float hz = baseH(x, z + 1.2f) - baseH(x, z - 1.2f);
                Vec3 n = Vec3(-hx, 2.4f, -hz).normalized();
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

        // Влажный бетонный плац осмотра техники с жёлтой технической разметкой (tag=4 -> отражения луж!)
        mb.addChamferBox({0.f, -0.04f, 0.f}, {22.f, 0.06f, 24.f}, 0.02f, Vec4(0.36f, 0.37f, 0.39f, 1.f), 0.55f, 0.08f, 4.f, 1.f);
        mb.addCylinder({0.f, 0.022f, 0.f}, 5.15f, 5.15f, 0.02f, 40, 1, Vec4(0.31f, 0.32f, 0.35f, 1.f), 0.42f, 0.12f, 4.f, 1.f);
        // Жёлтые полосы безопасности вокруг стоянки танка
        const Vec4 markYellow(0.86f, 0.66f, 0.14f, 1.f);
        mb.addChamferBox({-5.6f, 0.03f, 0.f}, {0.14f, 0.008f, 5.8f}, 0.f, markYellow, 0.6f, 0.1f, 3.f);
        mb.addChamferBox({ 5.6f, 0.03f, 0.f}, {0.14f, 0.008f, 5.8f}, 0.f, markYellow, 0.6f, 0.1f, 3.f);
        mb.addChamferBox({0.f, 0.03f, -5.8f}, {5.74f, 0.008f, 0.14f}, 0.f, markYellow, 0.6f, 0.1f, 3.f);
        mb.addChamferBox({0.f, 0.03f,  5.8f}, {5.74f, 0.008f, 0.14f}, 0.f, markYellow, 0.6f, 0.1f, 3.f);

        // Задние ремонтные боксы ангара, радарная вышка, топливные цистерны, бетонные блоки и сосны
        mb.addChamferBox({-14.f, 3.6f, 18.f}, {9.5f, 3.6f, 6.2f}, 0.12f, Vec4(0.34f, 0.36f, 0.35f, 1.f), 0.75f, 0.12f, 4.f, 0.92f);
        mb.addChamferBox({-14.f, 7.35f, 18.f}, {9.9f, 0.22f, 6.6f}, 0.05f, Vec4(0.24f, 0.26f, 0.28f, 1.f), 0.65f, 0.25f, 3.f);
        // Ворота ремонтных боксов
        for (int d = -1; d <= 1; ++d) {
            mb.addChamferBox({-14.f + d * 5.6f, 2.5f, 11.75f}, {2.1f, 2.5f, 0.08f}, 0.02f, Vec4(0.18f, 0.22f, 0.20f, 1.f), 0.6f, 0.35f, 3.f);
        }

        // КШМ / топливные цистерны и ящики боеприпасов
        mb.addCylinder({15.5f, 1.45f, 13.5f}, 1.25f, 1.25f, 5.6f, 20, 2, Vec4(0.28f, 0.34f, 0.24f, 1.f), 0.55f, 0.25f, 3.f);
        mb.addChamferBox({15.5f, 0.35f, 13.5f}, {1.35f, 0.35f, 2.6f}, 0.04f, Vec4(0.22f, 0.22f, 0.24f, 1.f), 0.7f, 0.2f, 4.f);
        for (int c = 0; c < 4; ++c) {
            mb.addChamferBox({9.5f + (c % 2) * 1.4f, 0.42f + (c / 2) * 0.82f, 9.5f}, {0.62f, 0.40f, 0.95f}, 0.03f,
                             Vec4(0.32f, 0.36f, 0.22f, 1.f), 0.72f, 0.1f, 5.f, 0.9f);
        }

        // Прожекторные мачты плаца и радиовышка
        for (int m = -1; m <= 1; m += 2) {
            mb.addCylinder({m * 18.5f, 5.5f, -12.f}, 0.18f, 0.11f, 11.f, 10, 1, Vec4(0.28f, 0.30f, 0.32f, 1.f), 0.5f, 0.5f, 3.f);
            mb.addChamferBox({m * 18.5f, 11.0f, -11.7f}, {0.85f, 0.28f, 0.22f}, 0.02f, Vec4(1.0f, 0.95f, 0.82f, 1.f), 0.1f, 0.1f, 6.f);
        }

        // Хвойный лес вокруг военной базы
        for (int i = 0; i < 42; ++i) {
            float ang = (float(i) / 42.f) * 2.f * PI;
            float rad = 32.f + (i % 5) * 9.5f;
            float tx = std::cos(ang) * rad;
            float tz = std::sin(ang) * rad;
            if (tz < -6.f && std::fabs(tx) < 16.f) continue; // открываем панорамный вид
            float ty = baseH(tx, tz);
            float hScale = 0.85f + (i % 4) * 0.22f;
            mb.addCylinder({tx, ty + 1.8f * hScale, tz}, 0.28f, 0.18f, 3.6f * hScale, 8, 1, Vec4(0.28f, 0.19f, 0.12f, 1.f), 0.9f, 0.02f, 5.f);
            mb.addCylinder({tx, ty + 4.4f * hScale, tz}, 2.2f * hScale, 0.2f, 4.2f * hScale, 9, 1, Vec4(0.13f, 0.24f, 0.13f, 1.f), 0.85f, 0.02f, 5.f);
            mb.addCylinder({tx, ty + 6.6f * hScale, tz}, 1.6f * hScale, 0.08f, 3.4f * hScale, 9, 1, Vec4(0.15f, 0.28f, 0.15f, 1.f), 0.85f, 0.02f, 5.f);
        }
        env.hangarOutdoor = mb.upload(dev);
    }

    // =========================================================================
    // 2. ЗАКРЫТЫЙ ЖЕЛЕЗОБЕТОННЫЙ АНГАР-БУНКЕР
    // =========================================================================
    {
        mb.clear();
        // Полированный бетонный пол с влажными бликами и поворотным кругом
        mb.addChamferBox({0.f, -0.05f, 0.f}, {18.f, 0.05f, 22.f}, 0.f, Vec4(0.24f, 0.25f, 0.27f, 1.f), 0.32f, 0.15f, 4.f);
        mb.addCylinder({0.f, 0.02f, 0.f}, 4.8f, 4.8f, 0.03f, 36, 1, Vec4(0.20f, 0.22f, 0.25f, 1.f), 0.28f, 0.22f, 4.f);
        // Стены и потолок с ребрами жёсткости и мостовым краном
        mb.addChamferBox({0.f, 9.2f, 0.f}, {18.f, 0.25f, 22.f}, 0.f, Vec4(0.18f, 0.19f, 0.21f, 1.f), 0.78f, 0.1f, 4.f);
        mb.addChamferBox({-17.5f, 4.5f, 0.f}, {0.5f, 4.6f, 22.f}, 0.f, Vec4(0.22f, 0.23f, 0.25f, 1.f), 0.75f, 0.1f, 4.f);
        mb.addChamferBox({ 17.5f, 4.5f, 0.f}, {0.5f, 4.6f, 22.f}, 0.f, Vec4(0.22f, 0.23f, 0.25f, 1.f), 0.75f, 0.1f, 4.f);
        mb.addChamferBox({0.f, 4.5f,  21.5f}, {18.f, 4.6f, 0.5f}, 0.f, Vec4(0.20f, 0.21f, 0.23f, 1.f), 0.75f, 0.1f, 4.f);
        mb.addChamferBox({0.f, 4.5f, -21.5f}, {18.f, 4.6f, 0.5f}, 0.f, Vec4(0.20f, 0.21f, 0.23f, 1.f), 0.75f, 0.1f, 4.f);

        for (int z = -16; z <= 16; z += 8) {
            mb.addChamferBox({-16.6f, 4.5f, (float)z}, {0.55f, 4.5f, 0.45f}, 0.04f, Vec4(0.28f, 0.29f, 0.31f, 1.f), 0.7f, 0.15f, 4.f);
            mb.addChamferBox({ 16.6f, 4.5f, (float)z}, {0.55f, 4.5f, 0.45f}, 0.04f, Vec4(0.28f, 0.29f, 0.31f, 1.f), 0.7f, 0.15f, 4.f);
            mb.addChamferBox({0.f, 8.6f, (float)z}, {16.8f, 0.32f, 0.35f}, 0.03f, Vec4(0.26f, 0.27f, 0.30f, 1.f), 0.6f, 0.3f, 3.f);
            // Светодиодные промышленные панели на потолке
            mb.addChamferBox({-4.5f, 8.22f, (float)z}, {2.2f, 0.06f, 0.28f}, 0.01f, Vec4(0.95f, 0.97f, 1.0f, 1.f), 0.1f, 0.1f, 6.f);
            mb.addChamferBox({ 4.5f, 8.22f, (float)z}, {2.2f, 0.06f, 0.28f}, 0.01f, Vec4(0.95f, 0.97f, 1.0f, 1.f), 0.1f, 0.1f, 6.f);
        }
        // Жёлтая балка мостового крана
        mb.addChamferBox({0.f, 7.6f, 4.f}, {16.4f, 0.35f, 0.45f}, 0.03f, Vec4(0.82f, 0.62f, 0.12f, 1.f), 0.5f, 0.35f, 3.f);
        env.hangarBunker = mb.upload(dev);
    }

    // =========================================================================
    // 3. ЧЕТЫРЕ БОЕВЫЕ КАРТЫ + ПОЛИГОН (ВЫСОКОДЕТАЛИЗИРОВАННЫЙ ЛАНДШАФТ + УКРЫТИЯ)
    // =========================================================================
    const auto& maps = getMaps();
    for (int m = 0; m < 4 && m < (int)maps.size(); ++m) {
        mb.clear();
        const MapSpec& ms = maps[m];
        const int N = 112;
        const float half = ms.size * 0.5f;
        Vec4 gCol(ms.groundColor.x, ms.groundColor.y, ms.groundColor.z, 1.f);

        std::vector<uint32_t> grid((N + 1) * (N + 1));
        for (int gz = 0; gz <= N; ++gz) {
            for (int gx = 0; gx <= N; ++gx) {
                float x = ((float(gx) / N) - 0.5f) * 2.f * half;
                float z = ((float(gz) / N) - 0.5f) * 2.f * half;
                float y = getTerrainHeight(m, x, z);
                float dx = getTerrainHeight(m, x + 1.5f, z) - getTerrainHeight(m, x - 1.5f, z);
                float dz = getTerrainHeight(m, x, z + 1.5f) - getTerrainHeight(m, x, z - 1.5f);
                Vec3 n = Vec3(-dx, 3.0f, -dz).normalized();

                // Грунтовая/асфальтовая дорога вдоль осей X=0 и Z=0
                bool onRoad = (std::fabs(x) < 5.2f || (!ms.isPoligon && std::fabs(z) < 5.2f));
                Vec4 vCol = onRoad ? Vec4(0.25f, 0.24f, 0.23f, 1.f) : gCol;
                grid[gz * (N + 1) + gx] = mb.addVert({x, y, z}, n, vCol, onRoad ? 0.72f : 0.88f, 0.04f, onRoad ? 4.f : 0.f, 1.f);
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

        // Здания, цеха, ДОТы, силосные цистерны и гранитные валуны из MapSpec::obstacles
        for (const auto& ob : ms.obstacles) {
            float by = getTerrainHeight(m, ob.x, ob.z);
            Mat4 obXf = Mat4::rotationY(ob.rot) * Mat4::translation(ob.x, by, ob.z);
            if (ob.kind == OB_ROCK) {
                mb.addEllipsoid({ob.x, by + ob.h * 0.35f, ob.z}, {ob.w * 0.55f, ob.h * 0.65f, ob.d * 0.55f},
                                8, 10, false, Vec4(0.36f, 0.35f, 0.34f, 1.f), 0.85f, 0.05f, 4.f, 0.9f);
            } else if (ob.kind == OB_BUNKER || ob.kind == OB_WALL) {
                mb.addChamferBox({0.f, ob.h * 0.5f - 0.3f, 0.f}, {ob.w * 0.5f, ob.h * 0.5f + 0.3f, ob.d * 0.5f},
                                 0.16f, Vec4(0.42f, 0.43f, 0.41f, 1.f), 0.80f, 0.08f, 4.f, 0.92f, &obXf);
            } else if (ob.kind == OB_SILO) {
                mb.addCylinder({ob.x, by + ob.h * 0.5f, ob.z}, ob.w * 0.48f, ob.w * 0.46f, ob.h, 18, 1,
                               Vec4(0.56f, 0.58f, 0.60f, 1.f), 0.52f, 0.35f, 3.f, 0.94f);
            } else {
                mb.addChamferBox({0.f, ob.h * 0.5f - 0.4f, 0.f}, {ob.w * 0.5f, ob.h * 0.5f + 0.4f, ob.d * 0.5f},
                                 0.12f, Vec4(0.48f, 0.45f, 0.41f, 1.f), 0.78f, 0.08f, 4.f, 0.94f, &obXf);
                mb.addChamferBox({0.f, ob.h + 0.18f, 0.f}, {ob.w * 0.52f, 0.22f, ob.d * 0.52f},
                                 0.06f, Vec4(0.32f, 0.24f, 0.21f, 1.f), 0.70f, 0.18f, 3.f, 0.96f, &obXf);
            }
        }

        // Деревья и ели по карте (кроме дорог и точек захвата)
        uint32_t tSeed = 1337u + m * 7919u;
        for (int t = 0; t < 68; ++t) {
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
            Vec4 foliage = (m == 2) ? Vec4(0.22f, 0.30f, 0.26f, 1.f) : Vec4(0.15f, 0.27f, 0.14f, 1.f);
            mb.addCylinder({tx, ty + 1.6f * sc, tz}, 0.26f, 0.16f, 3.2f * sc, 7, 1, Vec4(0.28f, 0.19f, 0.12f, 1.f), 0.9f, 0.02f, 5.f);
            mb.addCylinder({tx, ty + 4.2f * sc, tz}, 2.1f * sc, 0.15f, 4.4f * sc, 8, 1, foliage, 0.86f, 0.02f, 5.f);
            mb.addCylinder({tx, ty + 6.4f * sc, tz}, 1.5f * sc, 0.06f, 3.2f * sc, 8, 1, foliage, 0.86f, 0.02f, 5.f);
        }

        // Дистанционные рубежи стрельбища на Полигоне (100м, 200м, 300м, 400м)
        if (ms.isPoligon) {
            for (int r = 1; r <= 4; ++r) {
                float rz = 128.f - r * 70.f;
                float ry = getTerrainHeight(m, -36.f, rz);
                mb.addChamferBox({-36.f, ry + 1.4f, rz}, {2.2f, 1.1f, 0.12f}, 0.02f, Vec4(0.85f, 0.65f, 0.14f, 1.f), 0.5f, 0.2f, 3.f);
            }
        }
        env.mapTerrain[m] = mb.upload(dev);
    }

    // 4. Универсальные меши спецэффектов (взрывы/дымы, трассеры снарядов, кольцо точки захвата)
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
        if (i % 2 == 0) { // Пунктирный меловой круг точки захвата в стиле War Thunder!
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
