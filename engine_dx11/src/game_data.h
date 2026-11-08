#pragma once
#include "math3d.h"
#include <dag_ecs_world.h>
#include <vector>
#include <list>
#include <string>
#include <cstring>

namespace sf {

inline const char* internDagorStr(const std::string& s, const char* fallback) {
    if (s.empty()) return fallback;
    static std::list<std::string> pool;
    pool.push_back(s);
    return pool.back().c_str();
}

void initOrReloadDagorGamedata(bool isHotReload = false);

enum ShellType : int {
    SHELL_APFSDS = 0, // ОБПС
    SHELL_HEATFS = 1, // КС
    SHELL_ATGM   = 2, // ПТУР (управляемая ракета по лучу прицела)
    SHELL_HE     = 3  // ОФС / БФС
};

enum TankArch : int {
    ARCH_TSERIES = 0, // Советская / Российская / Сербская школа (Т-72Б3, Т-80У, Т-90М, M-84AS2)
    ARCH_CHINESE = 1, // Китайская школа (ZTZ-96B, VT-4A1, ZTZ-99A)
    ARCH_WESTERN = 2, // Западная школа НАТО (Leopard 2A7V, M1A2 SEP v3, Leclerc, Strv 122B+)
    ARCH_IFV     = 3  // Лёгкие танки и БМП-истребители (BVP M-80AB1, CV 90120-T, M10 Booker)
};

struct ShellSpec {
    const char* code;
    ShellType   type;
    const char* label;
    float       speedMs;
    float       damage;
    int         basePenMM;
    bool        guided;

    void getPenTable(int m0[3], int m200[3], int m500[3]) const {
        if (type == SHELL_APFSDS) {
            m0[0]   = basePenMM;
            m0[1]   = int(basePenMM * 0.87f);
            m0[2]   = int(basePenMM * 0.58f);
            m200[0] = int(basePenMM * 0.98f);
            m200[1] = int(basePenMM * 0.85f);
            m200[2] = int(basePenMM * 0.57f);
            m500[0] = int(basePenMM * 0.94f);
            m500[1] = int(basePenMM * 0.81f);
            m500[2] = int(basePenMM * 0.54f);
        } else {
            int p30 = int(basePenMM * 0.86f);
            int p60 = int(basePenMM * 0.50f);
            m0[0] = m200[0] = m500[0] = basePenMM;
            m0[1] = m200[1] = m500[1] = p30;
            m0[2] = m200[2] = m500[2] = p60;
        }
    }
};

struct NationSpec {
    int         idx;
    const char* id;
    const char* name;
    const char* subtitle;
    Vec3        accentColor;
};

struct VehicleSpec {
    int         idx;
    const char* id;
    int         nationIdx;
    const char* name;
    const char* shortName;
    const char* br;
    const char* rank;
    const char* roleClass;
    const char* gunName;
    const char* coaxName;
    const char* eraName;
    const char* history;
    float       reloadSec;
    int         crew;
    int         hp;
    int         hullArmor[3];   // лоб / борт / корма (мм)
    int         turretArmor[3]; // лоб / борт / корма (мм)
    float       fwdKmh;
    float       revKmh;
    int         engineHp;
    float       weightTons;
    float       turretDegSec;
    float       hullDegSec;
    bool        unlockedDefault;
    int         costRp;
    int         costSilver;
    TankArch    arch;
    Vec3        baseColor;
    bool        wedgeTurret;
    bool        hasLog;
    bool        hasFuelDrums;
    bool        hasAtgmRails;
    bool        hasSearchlight;
    bool        hasRcws;
    bool        hasBustle;
    bool        isTurbine;
    ShellSpec   shells[3];
};

struct CamoSpec {
    int         idx;
    const char* name;
    Vec3        c0;
    Vec3        c1;
    Vec3        c2;
    bool        useVehicleBase;
};

inline std::vector<NationSpec>& mutableNations() {
    static std::vector<NationSpec> nations = {
        {0, "ru", "РОССИЯ",    "СУХОПУТНЫЕ ВОЙСКА РФ",           {0.85f, 0.22f, 0.16f}},
        {1, "rs", "СЕРБИЯ",    "ВОЈСКА СРБИЈЕ • SERBIA MOD",     {0.78f, 0.20f, 0.24f}},
        {2, "cn", "КИТАЙ",     "СУХОПУТНЫЕ ВОЙСКА НОАК",         {0.87f, 0.23f, 0.14f}},
        {3, "us", "США",       "UNITED STATES ARMY",             {0.23f, 0.47f, 0.78f}},
        {4, "eu", "НАТО / ЕС", "ОБЪЕДИНЁННЫЙ ЕВРОКОРПУС НАТО",   {0.21f, 0.41f, 0.74f}},
        {5, "fr", "ФРАНЦИЯ",   "ARMÉE DE TERRE",                 {0.28f, 0.46f, 0.81f}},
        {6, "se", "ШВЕЦИЯ",    "PANSARTRUPPERNA ШВЕЦИИ",         {0.87f, 0.69f, 0.16f}}
    };
    return nations;
}

inline std::vector<CamoSpec>& mutableCamouflages() {
    static std::vector<CamoSpec> camos = {
        {0, "Армейский защитный (Базовый)", {0.26f,0.31f,0.22f}, {0.26f,0.31f,0.22f}, {0.26f,0.31f,0.22f}, true},
        {1, "Трёхцветный лесной НАТО",      {0.23f,0.28f,0.20f}, {0.15f,0.16f,0.15f}, {0.36f,0.27f,0.19f}, false},
        {2, "Балканский камуфляж М-84",     {0.24f,0.30f,0.21f}, {0.13f,0.16f,0.13f}, {0.42f,0.35f,0.23f}, false},
        {3, "Пустынный буря (Песочный)",    {0.65f,0.56f,0.41f}, {0.52f,0.44f,0.30f}, {0.38f,0.32f,0.23f}, false},
        {4, "Арктический зимний",           {0.80f,0.83f,0.85f}, {0.46f,0.51f,0.53f}, {0.24f,0.28f,0.26f}, false},
        {5, "Парадный тёмный антрацит",     {0.15f,0.17f,0.19f}, {0.10f,0.11f,0.13f}, {0.22f,0.24f,0.27f}, false}
    };
    return camos;
}

inline std::vector<VehicleSpec>& mutableVehicles() {
    static std::vector<VehicleSpec> v = {
        // 0..3 РОССИЯ
        {0, "t72b3", 0, "Т-72Б3 УБХ", "Т-72Б3", "10.7", "VII", "ОСНОВНОЙ ТАНК",
         "125мм 2А46М-5", "7.62мм ПКТМ • 12.7мм Корд", "Контакт-5 + Реликт",
         "Модернизация Т-72Б с многоканальным тепловизором «Сосна-У», двигателем В-92С2Ф (1130 л.с.) и бортовыми экранами ДЗ «Реликт».",
         7.1f, 3, 2350, {530,85,45}, {610,140,65}, 65.f, 4.5f, 1130, 46.5f, 36.f, 34.f, true, 0, 0,
         ARCH_TSERIES, {0.25f,0.30f,0.20f}, false, true, false, false, false, true, false, false,
         {{"3БМ60", SHELL_APFSDS, "ОБПС", 1660, 640, 580, false},
          {"3БК18М", SHELL_HEATFS, "КС",  905,  540, 550, false},
          {"9М119М1", SHELL_ATGM, "ПТУР", 340,  760, 850, true}}},

        {1, "t80u", 0, "Т-80У", "Т-80У", "11.3", "VII", "ОСНОВНОЙ ТАНК",
         "125мм 2А46М-1", "7.62мм ПКТ • 12.7мм НСВТ", "Контакт-5",
         "Знаменитый советский ОБТ с газотурбинным двигателем ГТД-1250 (1250 л.с.) и встроенной динамической защитой 2-го поколения «Контакт-5».",
         6.5f, 3, 2400, {550,80,50}, {630,145,65}, 70.f, 11.f, 1250, 46.0f, 35.f, 38.f, true, 0, 0,
         ARCH_TSERIES, {0.23f,0.29f,0.19f}, false, true, true, false, true, true, false, true,
         {{"3БМ46", SHELL_APFSDS, "ОБПС", 1700, 625, 532, false},
          {"3БК18М", SHELL_HEATFS, "КС",  905,  530, 550, false},
          {"9М119",  SHELL_ATGM,  "ПТУР", 345,  740, 750, true}}},

        {2, "t80bvm", 0, "Т-80БВМ", "Т-80БВМ", "11.7", "VIII", "ОСНОВНОЙ ТАНК",
         "125мм 2А46М-4", "7.62мм ПКТМ • 12.7мм НСВТ", "Реликт",
         "Скоростной реактивный штурмовой танк с модульной динамической защитой «Реликт», газотурбиной ГТД-1250ТФ и мгновенным разгоном.",
         6.5f, 3, 2450, {590,120,50}, {660,180,65}, 70.f, 12.f, 1250, 46.0f, 40.f, 40.f, true, 0, 0,
         ARCH_TSERIES, {0.21f,0.27f,0.18f}, false, true, false, false, false, true, false, true,
         {{"3БМ60", SHELL_APFSDS, "ОБПС", 1660, 650, 580, false},
          {"3БК29М", SHELL_HEATFS, "КС",  915,  560, 650, false},
          {"9М119М1", SHELL_ATGM, "ПТУР", 350,  780, 850, true}}},

        {3, "t90m", 0, "Т-90М «Прорыв»", "Т-90М", "12.0", "VIII", "ОСНОВНОЙ ТАНК",
         "125мм 2А46М-5-01", "7.62мм ПКТМ • 12.7мм ДУМВ Корд-МТ", "Реликт + Сварная башня",
         "Флагманский российский ОБТ со сварной башней, кормовым бронеотсеком БК, комплексом «Реликт» и панорамным тепловизором ПК ПАН.",
         7.1f, 3, 2550, {620,130,50}, {710,190,75}, 60.f, 4.5f, 1130, 48.0f, 40.f, 35.f, false, 45000, 280000,
         ARCH_TSERIES, {0.27f,0.29f,0.20f}, true, false, false, false, false, true, true, false,
         {{"3БМ60", SHELL_APFSDS, "ОБПС", 1680, 665, 590, false},
          {"3ОФ26", SHELL_HE,     "ОФС",  850,  820, 68,  false},
          {"9М119М1", SHELL_ATGM, "ПТУР", 355,  790, 850, true}}},

        // 4..7 СЕРБИЯ (SERBIA MOD)
        {4, "bvp_m80ab1", 1, "BVP M-80AB1", "M-80AB1", "9.7", "VI", "БМП / ИСТРЕБИТЕЛЬ",
         "30мм Zastava M86 + 2× Маљутка-2Т5", "7.62мм M86", "Модульная броня AB1",
         "Сербская глубокая модернизация БМП М-80А с тепловизионной СУО и пусковыми рельсами сверхзвуковых тандемных ПТУР «Маљутка 2Т5» (860 мм пробития).",
         3.4f, 3, 1850, {110,45,30}, {130,50,35}, 68.f, 14.f, 360, 15.8f, 48.f, 46.f, true, 0, 0,
         ARCH_IFV, {0.24f,0.30f,0.20f}, false, false, false, true, false, false, false, false,
         {{"30мм APFSDS", SHELL_APFSDS, "ОБПС", 1380, 420, 310, false},
          {"Маљутка-2Т5", SHELL_ATGM,   "ПТУР", 330,  830, 860, true},
          {"30мм ОФЗТ",   SHELL_HE,     "ОФЗ",  1100, 460, 85,  false}}},

        {5, "m84as1", 1, "M-84AS1", "M-84AS1", "11.0", "VII", "ОСНОВНОЙ ТАНК",
         "125мм 2А46М", "7.62мм M86 • 12.7мм ДУМВ M15", "ERO-19 Gen.2",
         "Модернизированный основной боевой танк Войска Сербии: сербская динамическая защита ERO-19, датчики лазерного облучения и боевой модуль М15.",
         6.8f, 3, 2400, {555,110,48}, {625,165,65}, 68.f, 8.f, 1000, 45.0f, 38.f, 37.f, true, 0, 0,
         ARCH_TSERIES, {0.23f,0.29f,0.20f}, false, false, true, false, false, true, false, false,
         {{"M88 ППОБ", SHELL_APFSDS,   "ОБПС", 1715, 635, 545, false},
          {"M88 ТФК",  SHELL_HEATFS,   "КС",   910,  545, 560, false},
          {"Рефлекс",  SHELL_ATGM,     "ПТУР", 345,  760, 780, true}}},

        {6, "m91_vihor", 1, "M-91 «Вихор»", "Вихор", "11.3", "VII", "ОСНОВНОЙ ТАНК",
         "125мм 2А46М-Вихор", "7.62мм M86 • 12.7мм M87", "Композит + ERO",
         "Югославско-сербский скоростной танк нового поколения с 1200-сильным дизелем V46-TK1 (27 л.с./т) и переработанной сварно-литой башней.",
         6.4f, 3, 2420, {560,95,50}, {640,155,70}, 72.f, 12.f, 1200, 44.0f, 42.f, 42.f, true, 0, 0,
         ARCH_TSERIES, {0.27f,0.31f,0.22f}, true, true, false, false, false, true, false, false,
         {{"M95 ОБПС", SHELL_APFSDS, "ОБПС", 1740, 645, 568, false},
          {"M91 КС",   SHELL_HEATFS, "КС",   915,  550, 580, false},
          {"M86 ОФС",  SHELL_HE,     "ОФС",  850,  800, 65,  false}}},

        {7, "m84as2", 1, "M-84AS2 «Војвода»", "M-84AS2", "12.0", "VIII", "ОСНОВНОЙ ТАНК",
         "125мм 2А46М-AS2", "7.62мм M86 • 12.7мм ДУМВ M20", "ERO-19M Тяжёлая",
         "Новейший флагман сербского танкостроения: плотное перекрытие ВЛД и башни модулями ERO-19M, панорамный тепловизор 3-го пок. и снаряд М20.",
         6.3f, 3, 2540, {615,135,55}, {695,195,75}, 70.f, 11.f, 1200, 46.5f, 42.f, 39.f, false, 40000, 250000,
         ARCH_TSERIES, {0.21f,0.28f,0.19f}, true, false, false, false, false, true, true, false,
         {{"M20 ОБПС",   SHELL_APFSDS, "ОБПС", 1750, 665, 605, false},
          {"M19 Тандем", SHELL_HEATFS, "КС",   930,  570, 660, false},
          {"Рефлекс-М",  SHELL_ATGM,   "ПТУР", 355,  800, 860, true}}},

        // 8..10 КИТАЙ
        {8, "ztz96b", 2, "ZTZ-96B", "ZTZ-96B", "10.7", "VII", "ОСНОВНОЙ ТАНК",
         "125мм ZPT-98", "7.62мм Type 86 • 12.7мм QJC-88", "FY-4",
         "Скоростная модификация китайского ОБТ ZTZ-96 с 1000-сильным дизелем, гидромеханической КПП и клиновидной башней FY-4.",
         7.1f, 3, 2360, {520,85,45}, {620,140,60}, 71.f, 15.f, 1000, 42.5f, 36.f, 38.f, true, 0, 0,
         ARCH_CHINESE, {0.32f,0.32f,0.22f}, true, false, true, false, false, true, true, false,
         {{"DTC10-125", SHELL_APFSDS, "ОБПС", 1760, 640, 577, false},
          {"DJB-125",   SHELL_HEATFS, "КС",   920,  540, 560, false},
          {"GP125",     SHELL_ATGM,   "ПТУР", 340,  755, 780, true}}},

        {9, "vt4", 2, "VT-4A1", "VT-4A1", "11.7", "VIII", "ОСНОВНОЙ ТАНК",
         "125мм ZPT-98E", "7.62мм Type 86 • 12.7мм ДУМВ", "FY-4E + КАЗ",
         "Современный китайский ОБТ 3-го поколения с цифровой СУО «охотник-стрелок», двигателем 1300 л.с. и модульной защитой FY-4E.",
         6.7f, 3, 2480, {575,110,50}, {665,175,65}, 70.f, 17.f, 1300, 52.0f, 40.f, 38.f, true, 0, 0,
         ARCH_CHINESE, {0.36f,0.34f,0.24f}, true, false, false, false, false, true, true, false,
         {{"BTA4 ОБПС", SHELL_APFSDS, "ОБПС", 1750, 650, 585, false},
          {"BTJ1 КС",   SHELL_HEATFS, "КС",   925,  555, 600, false},
          {"GP125-T",   SHELL_ATGM,   "ПТУР", 350,  785, 840, true}}},

        {10, "ztz99a", 2, "ZTZ-99A", "ZTZ-99A", "12.0", "VIII", "ОСНОВНОЙ ТАНК",
         "125мм ZPT-98A (L/50)", "7.62мм QJT-88 • 12.7мм QJC-88A", "FY-5 Тяжёлая",
         "Элитный тяжёлый ОБТ НОАК с двигателем 1500 л.с. (задний ход 34 км/ч!), тяжёлой ДЗ FY-5 и лазерным комплексом JD-3.",
         6.5f, 3, 2580, {630,125,55}, {720,190,75}, 75.f, 34.f, 1500, 55.0f, 40.f, 40.f, false, 45000, 290000,
         ARCH_CHINESE, {0.29f,0.32f,0.22f}, true, false, false, false, false, true, true, false,
         {{"DTC10-125", SHELL_APFSDS, "ОБПС", 1770, 665, 602, false},
          {"DJB-2",     SHELL_HEATFS, "КС",   930,  560, 620, false},
          {"GP125M",    SHELL_ATGM,   "ПТУР", 355,  790, 850, true}}},

        // 11..13 США
        {11, "m10", 3, "M10 Booker", "M10 Booker", "10.3", "VII", "ЛЁГКИЙ ТАНК",
         "105мм XM35", "7.62мм M240C • 12.7мм M2HB", "Модульная броня",
         "Машина огневой поддержки армии США со скорострельным 105-мм орудием (перезарядка 5.0с) и панорамным тепловизором CITV.",
         5.0f, 4, 2050, {220,65,40}, {280,80,45}, 72.f, 28.f, 800, 38.0f, 46.f, 44.f, true, 0, 0,
         ARCH_IFV, {0.31f,0.33f,0.25f}, false, false, false, false, false, true, true, false,
         {{"M900",    SHELL_APFSDS, "ОБПС", 1505, 575, 522, false},
          {"M456A2",  SHELL_HEATFS, "КС",   1170, 500, 440, false},
          {"M393A3",  SHELL_HE,     "БФС",   730, 720, 127, false}}},

        {12, "m1a1", 3, "M1A1HC Abrams", "M1A1HC", "11.3", "VII", "ОСНОВНОЙ ТАНК",
         "120мм M256", "7.62мм M240 • 12.7мм M2HB", "Chobham HAP-2",
         "Тяжёлая версия M1A1 с усиленными скулами башни, вышибными панелями боекомплекта и 1500-сильной газотурбиной AGT1500.",
         6.0f, 4, 2560, {450,95,45}, {660,150,55}, 68.f, 38.f, 1500, 61.2f, 40.f, 38.f, true, 0, 0,
         ARCH_WESTERN, {0.35f,0.34f,0.26f}, false, false, false, false, false, true, true, true,
         {{"M829A1", SHELL_APFSDS, "ОБПС",  1575, 645, 598, false},
          {"M830A1", SHELL_HEATFS, "КС-РВ", 1410, 560, 500, false},
          {"M908",   SHELL_HE,     "ББ-ОФ", 1410, 730, 185, false}}},

        {13, "m1a2sep", 3, "M1A2 SEP v3", "M1A2 SEP v3", "12.0", "VIII", "ОСНОВНОЙ ТАНК",
         "120мм M256E1", "7.62мм M240 • 12.7мм CROWS-LP", "NGAP + ARAT",
         "Вершина развития «Абрамса»: комбинированная броня нового поколения NGAP, модуль CROWS-LP, ТВП 3-го пок. и снаряд M829A2.",
         6.0f, 4, 2680, {540,125,50}, {730,185,60}, 68.f, 36.f, 1500, 66.8f, 42.f, 36.f, false, 45000, 295000,
         ARCH_WESTERN, {0.38f,0.37f,0.28f}, false, false, false, false, false, true, true, true,
         {{"M829A2", SHELL_APFSDS, "ОБПС",  1680, 670, 629, false},
          {"M830A1", SHELL_HEATFS, "КС",    1410, 570, 520, false},
          {"M1147",  SHELL_HE,     "ОФС-П", 1150, 810, 110, false}}},

        // 14..16 НАТО / ЕС
        {14, "ariete", 4, "C1 Ariete AMV", "Ariete AMV", "11.7", "VII", "ОСНОВНОЙ ТАНК",
         "120мм OTO Breda L/44", "7.62мм MG42/59 × 2", "WAR + PSO",
         "Итальянский ОБТ с форсированным дизелем IVECO V12 (1500 л.с.), пакетами бронирования WAR/PSO и пробивным снарядом DM53.",
         6.0f, 4, 2420, {460,105,45}, {610,160,55}, 70.f, 32.f, 1500, 57.0f, 42.f, 40.f, true, 0, 0,
         ARCH_WESTERN, {0.28f,0.31f,0.23f}, true, false, false, false, false, true, true, false,
         {{"DM53",   SHELL_APFSDS, "ОБПС", 1670, 655, 623, false},
          {"DM12A1", SHELL_HEATFS, "КС",   1140, 550, 480, false},
          {"DM11",   SHELL_HE,     "ОФС",   980, 790,  75, false}}},

        {15, "challenger2", 4, "Challenger 2 TES", "Challenger 2", "11.7", "VIII", "ОСНОВНОЙ ТАНК",
         "120мм L30A1 (нарезная)", "7.62мм L94A1 • 12.7мм Enforcer", "Dorchester 2E + NERA",
         "Британская бронекрепость в обвесе Theatre Entry Standard: мощнейшая круговая защита Dorchester 2E и перезарядка 5.3с.",
         5.3f, 4, 2720, {570,185,55}, {725,220,70}, 59.f, 36.f, 1200, 74.0f, 36.f, 32.f, true, 0, 0,
         ARCH_WESTERN, {0.28f,0.32f,0.24f}, false, false, false, false, false, true, true, false,
         {{"L27A1",  SHELL_APFSDS, "ОБПС", 1650, 650, 564, false},
          {"L31A7",  SHELL_HE,     "БФС",   670, 850, 152, false},
          {"L23A1",  SHELL_APFSDS, "ОБПС", 1535, 590, 480, false}}},

        {16, "leo2a7", 4, "Leopard 2A7V", "Leopard 2A7V", "12.0", "VIII", "ОСНОВНОЙ ТАНК",
         "120мм Rh-120 L/55A1", "7.62мм MG3A1 × 2", "NERA 3-го пок. + ВЛД",
         "Эталонный германский ОБТ с длинноствольным орудием L/55A1 (рекордные 652 мм пробития DM53) и клиновидными щеками башни.",
         6.0f, 4, 2660, {625,115,50}, {750,190,65}, 68.f, 31.f, 1500, 66.5f, 42.f, 38.f, false, 48000, 310000,
         ARCH_WESTERN, {0.27f,0.30f,0.22f}, true, false, false, false, false, false, true, false,
         {{"DM53 L/55", SHELL_APFSDS, "ОБПС", 1750, 680, 652, false},
          {"DM12A1",    SHELL_HEATFS, "КС",   1140, 560, 480, false},
          {"DM11",      SHELL_HE,     "ОФС",   990, 810,  80, false}}},

        // 17..19 ФРАНЦИЯ
        {17, "amx40", 5, "AMX-40", "AMX-40", "10.0", "VI", "ОСНОВНОЙ ТАНК",
         "120мм GIAT CN120-25 + 20мм M693", "20мм автопушка M693", "Разнесённая броня",
         "Стремительный французский танк со спаренной 20-мм автопушкой и 1300-сильным дизелем для агрессивных фланговых атак.",
         6.5f, 4, 2220, {330,75,40}, {410,105,45}, 72.f, 28.f, 1300, 43.7f, 40.f, 42.f, true, 0, 0,
         ARCH_WESTERN, {0.29f,0.33f,0.24f}, false, false, false, false, false, true, true, false,
         {{"OFL 120 G1", SHELL_APFSDS, "ОБПС", 1650, 600, 495, false},
          {"OCC 120 G1", SHELL_HEATFS, "КС",   1100, 540, 480, false},
          {"OE 120 F1",  SHELL_HE,     "ОФС",  1000, 760,  65, false}}},

        {18, "leclerc", 5, "Leclerc S2", "Leclerc S2", "11.7", "VIII", "ОСНОВНОЙ ТАНК",
         "120мм CN120-26 L/52", "12.7мм M2HB • 7.62мм AAN F1", "Модульная NERA",
         "Французский ОБТ с кормовым конвейерным автоматом заряжания (стабильные 5.0с перезарядки!) и двигателем Hyperbar 1500 л.с.",
         5.0f, 3, 2450, {520,100,45}, {635,160,55}, 71.f, 38.f, 1500, 56.3f, 42.f, 40.f, true, 0, 0,
         ARCH_WESTERN, {0.26f,0.31f,0.23f}, false, false, false, false, false, true, true, false,
         {{"OFL 120 F1", SHELL_APFSDS, "ОБПС", 1790, 645, 575, false},
          {"OCC 120 F1", SHELL_HEATFS, "КС",   1100, 550, 480, false},
          {"OE 120 F1",  SHELL_HE,     "ОФС",  1020, 780,  70, false}}},

        {19, "leclercxlr", 5, "Leclerc SXXI XLR", "Leclerc XLR", "12.0", "VIII", "ОСНОВНОЙ ТАНК",
         "120мм CN120-26 L/52", "12.7мм M2HB • 7.62мм ДУМВ T2B", "Титан-NERA + AZUR",
         "Модернизация программы SCORPION: титаново-композитные вставки башни, бортовая ДЗ AZUR и снаряд OFL 120 F2.",
         5.0f, 3, 2540, {570,145,50}, {685,185,65}, 72.f, 38.f, 1500, 57.6f, 44.f, 40.f, false, 45000, 290000,
         ARCH_WESTERN, {0.28f,0.32f,0.24f}, false, false, false, false, false, true, true, false,
         {{"OFL 120 F2", SHELL_APFSDS, "ОБПС", 1790, 660, 604, false},
          {"OCC 120 F1", SHELL_HEATFS, "КС",   1120, 560, 510, false},
          {"OE 120 МП",  SHELL_HE,     "ОФС",  1030, 815,  85, false}}},

        // 20..22 ШВЕЦИЯ
        {20, "cv90120", 6, "CV 90120-T Ghost", "CV 90120", "11.3", "VII", "ЛЁГКИЙ ТАНК",
         "120мм CTG L/50", "7.62мм ksp 58", "Стелс-плиты Adaptiv",
         "Сверхбыстрый шведский истребитель танков (78 км/ч!) со 120-мм гладкоствольным орудием и автоматом заряжания 5.0 секунд.",
         5.0f, 4, 2020, {140,55,35}, {175,65,40}, 78.f, 42.f, 800, 27.7f, 48.f, 48.f, true, 0, 0,
         ARCH_IFV, {0.26f,0.32f,0.24f}, true, false, false, false, false, false, true, false,
         {{"slpprj m/95", SHELL_APFSDS, "ОБПС", 1700, 645, 598, false},
          {"slsgr m/95",  SHELL_HE,     "ОФС",   980, 790,  75, false},
          {"slpsgr m/95", SHELL_HEATFS, "КС",   1140, 545, 480, false}}},

        {21, "strv121", 6, "Strv 121", "Strv 121", "10.7", "VII", "ОСНОВНОЙ ТАНК",
         "120мм kan Strv 121 (L/44)", "7.62мм ksp 94 × 2", "Многослойная NERA",
         "Шведская версия Leopard 2A4 с гранатомётами Galix, высокой задней скоростью 31 км/ч и точной СУО EMES-15.",
         6.0f, 4, 2520, {450,95,45}, {560,145,55}, 68.f, 31.f, 1500, 55.2f, 40.f, 38.f, true, 0, 0,
         ARCH_WESTERN, {0.26f,0.30f,0.22f}, false, false, false, false, false, false, true, false,
         {{"slpprj m/90A", SHELL_APFSDS, "ОБПС", 1650, 615, 515, false},
          {"slpsgr m/88",  SHELL_HEATFS, "КС",   1140, 540, 480, false},
          {"slsgr m/98",   SHELL_HE,     "ОФС",   950, 760,  65, false}}},

        {22, "strv122", 6, "Strv 122B+ Evolution", "Strv 122B+", "12.0", "VIII", "ОСНОВНОЙ ТАНК",
         "120мм kan Strv 122 (L/44)", "7.62мм ksp 94 × 2", "Åkers Krutbruk NERA+",
         "Один из самых бронированных танков мира: тяжёлые шведские накладки ВЛД (670 мм!) и усиленная броня скул башни (765 мм!).",
         6.0f, 4, 2740, {670,155,55}, {765,215,70}, 68.f, 31.f, 1500, 64.5f, 42.f, 38.f, false, 48000, 310000,
         ARCH_WESTERN, {0.27f,0.32f,0.23f}, true, false, false, false, false, true, true, false,
         {{"slpprj m/95", SHELL_APFSDS, "ОБПС", 1680, 660, 595, false},
          {"slpsgr m/95", SHELL_HEATFS, "КС",   1140, 555, 480, false},
          {"slsgr m/01",  SHELL_HE,     "ОФС",   980, 805,  80, false}}}
    };
    return v;
}

// ==================== КАРТЫ, ВЫСОТЫ И УКРЫТИЯ ====================
enum ObstacleKind : int {
    OB_BUNKER   = 0,
    OB_BUILDING = 1,
    OB_WALL     = 2,
    OB_ROCK     = 3,
    OB_SILO     = 4
};

struct MapObstacle {
    ObstacleKind kind;
    float x, z, w, h, d, rot;
};

struct CaptureZoneDef {
    char  id;
    const char* name;
    float x, z, radius;
};

struct SpawnPoint {
    float x, z, yaw;
};

struct MapSpec {
    int   idx;
    const char* id;
    const char* name;
    const char* subtitle;
    float size;
    Vec3  skyTop;
    Vec3  skyHorizon;
    Vec3  groundColor;
    bool  isPoligon;
    std::vector<SpawnPoint> blueSpawns;
    std::vector<SpawnPoint> redSpawns;
    std::vector<CaptureZoneDef> zones;
    std::vector<MapObstacle> obstacles;
    std::vector<Vec2> checkpoints;
};

inline std::vector<MapSpec>& mutableMaps() {
    static std::vector<MapSpec> maps = {
        {
            0, "balkan_pass", "Балканский укрепрайон", "Онлайн Превосходство [A / B / C]", 360.f,
            {0.14f, 0.24f, 0.36f}, {0.56f, 0.66f, 0.75f}, {0.22f, 0.28f, 0.17f}, false,
            {{-125.f, 120.f, -0.75f}, {-105.f, 132.f, -0.78f}, {-138.f, 102.f, -0.70f}, {-86.f, 136.f, -0.82f}},
            {{ 125.f,-120.f,  2.38f}, { 105.f,-132.f,  2.35f}, { 138.f,-102.f,  2.42f}, { 86.f,-136.f,  2.30f}},
            {{'A', "Северная застава", -72.f, -48.f, 18.f},
             {'B', "Центральный перевал", 0.f, 0.f, 20.f},
             {'C', "Южный бункер", 72.f, 48.f, 18.f}},
            {{OB_BUNKER, -18, -14, 10, 3.6f, 8, 0.3f}, {OB_BUNKER, 20, 15, 10, 3.6f, 8, -0.25f},
             {OB_WALL, 0, -26, 14, 2.1f, 2.2f, 0.1f},  {OB_WALL, 0, 26, 14, 2.1f, 2.2f, 0.1f},
             {OB_BUILDING, -92, -56, 14, 6.5f, 11, 0.2f}, {OB_WALL, -56, -38, 11, 2.0f, 2.2f, -0.6f},
             {OB_ROCK, -64, -72, 9, 4.2f, 8, 0.5f}, {OB_BUNKER, 92, 54, 13, 4.4f, 10, -0.3f},
             {OB_WALL, 56, 36, 11, 2.0f, 2.2f, 0.6f}, {OB_ROCK, 66, 74, 9, 4.0f, 8, 0.9f},
             {OB_BUILDING, -45, 55, 16, 6.0f, 10, 0.4f}, {OB_BUILDING, 45, -55, 16, 6.0f, 10, 0.4f},
             {OB_SILO, -28, 86, 7, 7.5f, 7, 0.f}, {OB_SILO, 28, -86, 7, 7.5f, 7, 0.f}},
            {}
        },
        {
            1, "factory_18", "Промзона «Завод-18»", "Онлайн Превосходство [A / B / C]", 340.f,
            {0.17f, 0.20f, 0.25f}, {0.58f, 0.55f, 0.51f}, {0.25f, 0.24f, 0.22f}, false,
            {{-130.f, 0.f, -PI*0.5f}, {-130.f, 26.f, -PI*0.5f}, {-130.f, -26.f, -PI*0.5f}, {-118.f, 48.f, -PI*0.5f}},
            {{ 130.f, 0.f,  PI*0.5f}, { 130.f, 26.f,  PI*0.5f}, { 130.f, -26.f,  PI*0.5f}, { 118.f,-48.f,  PI*0.5f}},
            {{'A', "Ж/Д Депо", 0.f, -76.f, 18.f},
             {'B', "Сборочный цех", 0.f, 0.f, 20.f},
             {'C', "Хранилище ГСМ", 0.f, 76.f, 18.f}},
            {{OB_BUILDING, -28, -24, 18, 8.5f, 12, 0}, {OB_BUILDING, 28, 24, 18, 8.5f, 12, 0},
             {OB_BUILDING, -28,  26, 16, 7.5f, 11, 0}, {OB_BUILDING, 28,-26, 16, 7.5f, 11, 0},
             {OB_WALL, -16, 0, 3, 2.2f, 12, 0}, {OB_WALL, 16, 0, 3, 2.2f, 12, 0},
             {OB_BUILDING, -42, -78, 20, 7.0f, 12, 0}, {OB_BUILDING, 42, -78, 20, 7.0f, 12, 0},
             {OB_SILO, -24, 86, 9, 9.0f, 9, 0}, {OB_SILO, 24, 86, 9, 9.0f, 9, 0},
             {OB_BUNKER, -68, -36, 12, 4.0f, 10, 0}, {OB_BUNKER, 68, 36, 12, 4.0f, 10, 0}},
            {}
        },
        {
            2, "karelia_ridge", "Северный рубеж", "Онлайн Превосходство [A / B / C]", 380.f,
            {0.11f, 0.20f, 0.31f}, {0.62f, 0.71f, 0.78f}, {0.20f, 0.27f, 0.20f}, false,
            {{-140.f, 115.f, -0.8f}, {-120.f, 135.f, -0.8f}, {-148.f, 92.f, -0.75f}, {-96.f, 142.f, -0.85f}},
            {{ 140.f,-115.f, 2.34f}, { 120.f,-135.f, 2.34f}, { 148.f,-92.f,  2.39f}, { 96.f,-142.f,  2.28f}},
            {{'A', "Западное плато", -85.f, -20.f, 18.f},
             {'B', "Озёрная котловина", 0.f, 0.f, 22.f},
             {'C', "Восточный хребет", 85.f, 20.f, 18.f}},
            {{OB_ROCK, -24, -18, 12, 5.5f, 10, 0.4f}, {OB_ROCK, 24, 18, 12, 5.5f, 10, 1.2f},
             {OB_ROCK, -96, -42, 14, 6.2f, 11, 0.7f}, {OB_ROCK, 96, 42, 14, 6.2f, 11, 0.2f},
             {OB_BUNKER, -70, 10, 10, 3.8f, 9, 0.2f}, {OB_BUNKER, 70, -10, 10, 3.8f, 9, -0.2f},
             {OB_ROCK, -45, 68, 13, 5.0f, 10, 0.9f},  {OB_ROCK, 45, -68, 13, 5.0f, 10, 0.5f}},
            {}
        },
        {
            3, "poligon", "Испытательный полигон", "Стрельбище бронецелей + Трасса на время", 360.f,
            {0.13f, 0.24f, 0.37f}, {0.61f, 0.71f, 0.79f}, {0.23f, 0.29f, 0.19f}, true,
            {{0.f, 115.f, 0.f}},
            {},
            {},
            {{OB_BUNKER, -26, 98, 10, 3.5f, 8, 0}, {OB_BUILDING, 34, 104, 14, 5.8f, 10, 0},
             {OB_WALL, -14, 86, 8, 1.4f, 1.5f, 0}, {OB_WALL, 14, 86, 8, 1.4f, 1.5f, 0},
             {OB_ROCK, 76, 10, 10, 4.5f, 9, 0.4f}, {OB_ROCK, -78, -15, 11, 4.8f, 9, 0.8f}},
            {{0.f, 68.f}, {42.f, 32.f}, {64.f, -28.f}, {18.f, -78.f}, {-48.f, -46.f}, {-36.f, 38.f}}
        }
    };
    return maps;
}

inline ShellType parseBlkShellType(const std::string& s, ShellType def) {
    if (s == "apfsds") return SHELL_APFSDS;
    if (s == "heatfs") return SHELL_HEATFS;
    if (s == "atgm")   return SHELL_ATGM;
    if (s == "he")     return SHELL_HE;
    return def;
}

inline TankArch parseBlkArch(const std::string& s, TankArch def) {
    if (s == "tseries") return ARCH_TSERIES;
    if (s == "chinese") return ARCH_CHINESE;
    if (s == "western") return ARCH_WESTERN;
    if (s == "ifv")     return ARCH_IFV;
    return def;
}

inline void initOrReloadDagorGamedata(bool isHotReload) {
    static bool sLoadedOnce = false;
    if (sLoadedOnce && !isHotReload) return;
    sLoadedOnce = true;

    auto& kernel = dagor::RuntimeKernel::instance();
    kernel.mountAll();
    if (isHotReload) ++kernel.hotReloadCount;

    auto& nations = mutableNations();
    auto& camos   = mutableCamouflages();
    auto& vehs    = mutableVehicles();
    auto& maps    = mutableMaps();

    // 1. Загрузка бронетехники из gamedata/units/tanks_all_nations.blk (живой диск или VROMFS)
    dagor::DataBlockLite unitsRoot;
    bool fromLoose = false;
    if (kernel.vfs.loadBlk("gamedata/units/tanks_all_nations.blk", unitsRoot, &fromLoose)) {
        kernel.loadedLooseBlk = fromLoose;
        if (const auto* nb = unitsRoot.getBlockByName("nations")) {
            for (auto& n : nations) {
                if (const auto* b = nb->getBlockByName(n.id)) {
                    n.name = internDagorStr(b->getStr("name", n.name), n.name);
                    n.subtitle = internDagorStr(b->getStr("subtitle", n.subtitle), n.subtitle);
                    n.accentColor = Vec3(b->getPoint3("accentColor", n.accentColor.toDagor()));
                }
            }
        }
        if (const auto* vb = unitsRoot.getBlockByName("vehicles")) {
            int count = 0;
            for (auto& v : vehs) {
                kernel.entityMgr.registerTemplate(v.id);
                const auto* b = vb->getBlockByName(v.id);
                if (!b) continue;
                ++count;
                v.nationIdx       = b->getInt("nationIdx", v.nationIdx);
                v.name            = internDagorStr(b->getStr("name", v.name), v.name);
                v.shortName       = internDagorStr(b->getStr("shortName", v.shortName), v.shortName);
                v.br              = internDagorStr(b->getStr("br", v.br), v.br);
                v.rank            = internDagorStr(b->getStr("rank", v.rank), v.rank);
                v.roleClass       = internDagorStr(b->getStr("roleClass", v.roleClass), v.roleClass);
                v.gunName         = internDagorStr(b->getStr("gunName", v.gunName), v.gunName);
                v.coaxName        = internDagorStr(b->getStr("coaxName", v.coaxName), v.coaxName);
                v.eraName         = internDagorStr(b->getStr("eraName", v.eraName), v.eraName);
                v.history         = internDagorStr(b->getStr("history", v.history), v.history);
                v.reloadSec       = b->getReal("reloadSec", v.reloadSec);
                v.crew            = b->getInt("crew", v.crew);
                v.hp              = b->getInt("hp", v.hp);

                auto ha = b->getIPoint3("hullArmor", {v.hullArmor[0], v.hullArmor[1], v.hullArmor[2]});
                v.hullArmor[0] = ha.x; v.hullArmor[1] = ha.y; v.hullArmor[2] = ha.z;
                auto ta = b->getIPoint3("turretArmor", {v.turretArmor[0], v.turretArmor[1], v.turretArmor[2]});
                v.turretArmor[0] = ta.x; v.turretArmor[1] = ta.y; v.turretArmor[2] = ta.z;

                v.fwdKmh          = b->getReal("fwdKmh", v.fwdKmh);
                v.revKmh          = b->getReal("revKmh", v.revKmh);
                v.engineHp        = b->getInt("engineHp", v.engineHp);
                v.weightTons      = b->getReal("weightTons", v.weightTons);
                v.turretDegSec    = b->getReal("turretDegSec", v.turretDegSec);
                v.hullDegSec      = b->getReal("hullDegSec", v.hullDegSec);
                v.unlockedDefault = b->getBool("unlockedDefault", v.unlockedDefault);
                v.costRp          = b->getInt("costRp", v.costRp);
                v.costSilver      = b->getInt("costSilver", v.costSilver);
                v.arch            = parseBlkArch(b->getStr("arch", ""), v.arch);
                v.baseColor       = Vec3(b->getPoint3("baseColor", v.baseColor.toDagor()));
                v.wedgeTurret     = b->getBool("wedgeTurret", v.wedgeTurret);
                v.hasLog          = b->getBool("hasLog", v.hasLog);
                v.hasFuelDrums    = b->getBool("hasFuelDrums", v.hasFuelDrums);
                v.hasAtgmRails    = b->getBool("hasAtgmRails", v.hasAtgmRails);
                v.hasSearchlight  = b->getBool("hasSearchlight", v.hasSearchlight);
                v.hasRcws         = b->getBool("hasRcws", v.hasRcws);
                v.hasBustle       = b->getBool("hasBustle", v.hasBustle);
                v.isTurbine       = b->getBool("isTurbine", v.isTurbine);

                for (int s = 0; s < 3; ++s) {
                    char sKey[16];
                    std::snprintf(sKey, sizeof(sKey), "shell%d", s);
                    if (const auto* sb = b->getBlockByName(sKey)) {
                        v.shells[s].code      = internDagorStr(sb->getStr("code", v.shells[s].code), v.shells[s].code);
                        v.shells[s].type      = parseBlkShellType(sb->getStr("type", ""), v.shells[s].type);
                        v.shells[s].label     = internDagorStr(sb->getStr("label", v.shells[s].label), v.shells[s].label);
                        v.shells[s].speedMs   = sb->getReal("speedMs", v.shells[s].speedMs);
                        v.shells[s].damage    = sb->getReal("damage", v.shells[s].damage);
                        v.shells[s].basePenMM = sb->getInt("basePenMM", v.shells[s].basePenMM);
                        v.shells[s].guided    = sb->getBool("guided", v.shells[s].guided);
                    }
                }
            }
            kernel.loadedVehiclesBlk = count;
        }
    }

    // 2. Загрузка камуфляжей из gamedata/camouflages.blk
    dagor::DataBlockLite camosRoot;
    if (kernel.vfs.loadBlk("gamedata/camouflages.blk", camosRoot)) {
        if (const auto* cb = camosRoot.getBlockByName("camouflages")) {
            for (size_t i = 0; i < camos.size(); ++i) {
                char cKey[16];
                std::snprintf(cKey, sizeof(cKey), "camo%zu", i);
                if (const auto* b = cb->getBlockByName(cKey)) {
                    camos[i].name = internDagorStr(b->getStr("name", camos[i].name), camos[i].name);
                    camos[i].c0   = Vec3(b->getPoint3("c0", camos[i].c0.toDagor()));
                    camos[i].c1   = Vec3(b->getPoint3("c1", camos[i].c1.toDagor()));
                    camos[i].c2   = Vec3(b->getPoint3("c2", camos[i].c2.toDagor()));
                    camos[i].useVehicleBase = b->getBool("useVehicleBase", camos[i].useVehicleBase);
                }
            }
        }
    }

    // 3. Загрузка локаций и атмосферы daSkies из gamedata/levels/levels_steel_front.blk
    dagor::DataBlockLite levelsRoot;
    if (kernel.vfs.loadBlk("gamedata/levels/levels_steel_front.blk", levelsRoot)) {
        if (const auto* lb = levelsRoot.getBlockByName("levels")) {
            int lCount = 0;
            for (auto& m : maps) {
                if (const auto* b = lb->getBlockByName(m.id)) {
                    ++lCount;
                    m.name        = internDagorStr(b->getStr("name", m.name), m.name);
                    m.subtitle    = internDagorStr(b->getStr("subtitle", m.subtitle), m.subtitle);
                    m.size        = b->getReal("size", m.size);
                    m.skyTop      = Vec3(b->getPoint3("skyTop", m.skyTop.toDagor()));
                    m.skyHorizon  = Vec3(b->getPoint3("skyHorizon", m.skyHorizon.toDagor()));
                    m.groundColor = Vec3(b->getPoint3("groundColor", m.groundColor.toDagor()));
                }
            }
            kernel.loadedLevelsBlk = lCount;
        }
    }

    kernel.logLine("[DAGOR 6.5] Loaded DataBlocks: vehicles=%d, levels=%d, daECS_templates=%d, source=%s",
                   kernel.loadedVehiclesBlk, kernel.loadedLevelsBlk, kernel.entityMgr.templateCount(),
                   kernel.loadedLooseBlk ? "LOOSE_.BLK (prog/gameBase)" : "VROMFS (gamedata.vromfs.bin)");
}

inline const std::vector<NationSpec>& getNations() {
    initOrReloadDagorGamedata(false);
    return mutableNations();
}

inline const std::vector<CamoSpec>& getCamouflages() {
    initOrReloadDagorGamedata(false);
    return mutableCamouflages();
}

inline const std::vector<VehicleSpec>& getVehicles() {
    initOrReloadDagorGamedata(false);
    return mutableVehicles();
}

inline const std::vector<MapSpec>& getMaps() {
    initOrReloadDagorGamedata(false);
    return mutableMaps();
}

inline float getTerrainHeight(int mapIdx, float x, float z) {
    if (mapIdx == 1) { // factory_18
        float dist = std::hypot(x, z);
        float edge = std::max(0.f, (dist - 115.f) * 0.08f) * std::sin(x * 0.03f) * std::cos(z * 0.03f);
        return edge + std::sin(x * 0.025f) * std::cos(z * 0.025f) * 0.32f;
    }
    if (mapIdx == 3) { // poligon
        float corridor = clampf((std::fabs(x) - 36.f) / 45.f, 0.f, 1.f);
        return (std::sin(x * 0.024f) * std::cos(z * 0.022f) * 3.4f + std::sin((x + z) * 0.015f) * 2.0f) * corridor;
    }
    if (mapIdx == 2) { // karelia_ridge
        float cf = std::min(1.f, std::hypot(x, z) / 36.f);
        float h = std::sin(x * 0.022f) * std::cos(z * 0.021f) * 4.8f + std::sin(x * 0.045f + 1.2f) * std::sin(z * 0.042f) * 1.8f;
        return h * (0.28f + 0.72f * cf);
    }
    // balkan_pass
    float roadMask = std::min(1.f, std::fabs(x + z * 0.85f) / 26.f);
    float base = std::sin(x * 0.021f) * std::cos(z * 0.024f) * 3.8f + std::cos((x - z) * 0.016f) * 1.8f;
    return base * (0.22f + 0.78f * roadMask);
}

inline bool resolveMapCollision(const MapSpec& map, float& ioX, float& ioZ, float radius = 2.4f) {
    bool hit = false;
    float half = map.size * 0.47f;
    if (ioX < -half) { ioX = -half; hit = true; }
    if (ioX >  half) { ioX =  half; hit = true; }
    if (ioZ < -half) { ioZ = -half; hit = true; }
    if (ioZ >  half) { ioZ =  half; hit = true; }

    for (const auto& ob : map.obstacles) {
        float c = std::cos(-ob.rot), s = std::sin(-ob.rot);
        float dx = ioX - ob.x, dz = ioZ - ob.z;
        float lx = dx * c - dz * s;
        float lz = dx * s + dz * c;
        float hw = ob.w * 0.5f + radius;
        float hd = ob.d * 0.5f + radius;
        if (std::fabs(lx) < hw && std::fabs(lz) < hd) {
            hit = true;
            float px = hw - std::fabs(lx);
            float pz = hd - std::fabs(lz);
            if (px < pz) lx = (lx >= 0.f ? hw : -hw);
            else         lz = (lz >= 0.f ? hd : -hd);
            float rc = std::cos(ob.rot), rs = std::sin(ob.rot);
            ioX = ob.x + lx * rc - lz * rs;
            ioZ = ob.z + lx * rs + lz * rc;
        }
    }
    return hit;
}

inline bool checkSegmentBlocked(const MapSpec& map, const Vec3& a, const Vec3& b, Vec3* outHit = nullptr, bool* outGround = nullptr) {
    const int steps = 8;
    for (int i = 1; i <= steps; ++i) {
        float t = float(i) / float(steps);
        Vec3 p = Vec3::lerp(a, b, t);
        float gy = getTerrainHeight(map.idx, p.x, p.z);
        if (p.y <= gy) {
            if (outHit) *outHit = Vec3(p.x, gy, p.z);
            if (outGround) *outGround = true;
            return true;
        }
        for (const auto& ob : map.obstacles) {
            float baseH = getTerrainHeight(map.idx, ob.x, ob.z);
            if (p.y > baseH + ob.h) continue;
            float c = std::cos(-ob.rot), s = std::sin(-ob.rot);
            float dx = p.x - ob.x, dz = p.z - ob.z;
            float lx = dx * c - dz * s;
            float lz = dx * s + dz * c;
            if (std::fabs(lx) <= ob.w * 0.5f && std::fabs(lz) <= ob.d * 0.5f) {
                if (outHit) *outHit = p;
                if (outGround) *outGround = false;
                return true;
            }
        }
    }
    return false;
}

} // namespace sf
