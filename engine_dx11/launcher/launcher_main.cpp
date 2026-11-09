#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define DAGOR_ENABLE_QUIRREL_VM 1
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <winhttp.h>

#include "../src/math3d.h"
#include "../src/game_data.h"
#include "../src/d3d11_renderer.h"
#include "../src/tank_sculptor.h"
#include "../src/ui_d3d11.h"
#include "../src/settings_profile.h"

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

using namespace sf;

// Автозагрузка файлов ядра Dagor Engine 6.5 с GitHub CDN (работает даже при скачивании одного SteelFrontLauncher.exe!)
static const wchar_t* kGithubHost    = L"raw.githubusercontent.com";
static const wchar_t* kAppBlkUrlPath = L"/hhdrhkvcd/serbia_MOD/arena/2571b3b1-serbia-mod/application.blk";
static const wchar_t* kVromfsUrlPath = L"/hhdrhkvcd/serbia_MOD/arena/2571b3b1-serbia-mod/game/res/gamedata.vromfs.bin";
static const wchar_t* kSetupPsUrlPath= L"/hhdrhkvcd/serbia_MOD/arena/2571b3b1-serbia-mod/SETUP_FULL_DAGOR_ENGINE.ps1";
static const wchar_t* kGameUrlPath   = L"/hhdrhkvcd/serbia_MOD/arena/2571b3b1-serbia-mod/bin/win64/SteelFrontGame.exe";
static const wchar_t* kServerUrlPath = L"/hhdrhkvcd/serbia_MOD/arena/2571b3b1-serbia-mod/bin/win64/SteelFrontServer.exe";

struct LauncherHitBox {
    int id = 0;
    int param = 0;
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float mx, float my) const {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    }
};

enum LauncherAction {
    LACT_CLOSE = 1,
    LACT_MINIMIZE,
    LACT_QUALITY,
    LACT_DISPLAY,
    LACT_TOGGLE_MSAA,
    LACT_TOGGLE_SHADOWS,
    LACT_TOGGLE_VSYNC,
    LACT_TOGGLE_SOUND,
    LACT_EDIT_CALLSIGN,
    LACT_EDIT_SERVER_IP,
    LACT_RESET_LOCAL_IP,
    LACT_VERIFY_FILES,
    LACT_START_SERVER,
    LACT_OPEN_DAGOR_DIR,
    LACT_SELECT_DAGOR_MODE,
    LACT_PREV_SHOWCASE,
    LACT_NEXT_SHOWCASE,
    LACT_CYCLE_CAMO,
    LACT_PLAY
};

enum LauncherPhase {
    PHASE_STARTUP_CHECK  = 0, // Многоэтапная проверка при открытии лаунчера (~4.0 с)
    PHASE_DOWNLOADING    = 1, // Автозагрузка с GitHub CDN (если exe отсутствует или обновляется)
    PHASE_READY          = 2, // Готов к запуску игры
    PHASE_LAUNCHING_GAME = 3  // Плавная подготовка и старт клиента (~2.9 с)
};

class D3DLauncherApp {
public:
    using Color4 = sf::Color4;
    static constexpr int W = 1160;
    static constexpr int H = 700;

    HWND hwnd = nullptr;
    bool running = true;
    GameSettings cfg;

    D3D11Renderer gfx;
    UiRendererD3D11 ui;
    EnvironmentGpuAssets env;

    // Индексы витринных машин: M-84AS2 «Војвода», M-91 Vihor, Т-90М «Прорыв», M1A2 SEP v3, Leopard 2A7V, ZTZ-99A
    std::vector<int> showcaseVehIndices = {7, 6, 3, 13, 16, 10};
    std::vector<VehicleGpuModel> showcaseModels;
    VehicleGpuModel escortModelA;
    VehicleGpuModel escortModelB;
    int showcasePos = 0;
    int showcaseCamo = 0;

    // 3D камера живой диорамы в лаунчере
    float camYaw = 0.62f;
    float camPitch = 0.20f;
    float camDist = 7.7f;
    bool  dragging3D = false;
    POINT lastDragPt{0, 0};

    float mouseX = 0.f, mouseY = 0.f;
    int   activeEditField = 0; // 0=нет, 1=callsign, 2=serverIp
    int   dagorEngineMode = 0; // 0=Dagor 6.5 RT (.blk+VROMFS), 1=Gaijin daNetGame CDN, 2=Full Dagor SDK (Git+Jam)
    std::vector<LauncherHitBox> hitBoxes;

    // Состояние прогресса проверки / загрузки / старта
    LauncherPhase phase = PHASE_STARTUP_CHECK;
    double appTime = 0.0;
    float  phaseTimer = 0.f;
    float  displayPct = 0.f;

    std::atomic<bool> netBusy{false};
    std::atomic<int>  netPct{0};
    std::atomic<int>  netStage{0};
    std::atomic<bool> netOk{true};
    bool clientPresent = false;

    bool init(HWND window) {
        hwnd = window;
        char exePath[MAX_PATH] = {};
        ::GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        char* sl = std::strrchr(exePath, '\\');
        if (sl) *sl = 0;
        ::SetCurrentDirectoryA(exePath);
        if (::GetFileAttributesA("..\\..\\SteelFrontLauncher.exe") != INVALID_FILE_ATTRIBUTES) {
            ::SetCurrentDirectoryA("..\\..");
        }

        cfg.load();
        initOrReloadDagorGamedata(false);

        if (!gfx.init(hwnd, W, H, 4, true, true)) {
            return false;
        }
        if (!ui.init(gfx.device, gfx.ctx)) {
            return false;
        }

        const auto& vehs = getVehicles();
        showcaseModels.resize(showcaseVehIndices.size());
        for (size_t i = 0; i < showcaseVehIndices.size(); ++i) {
            buildVehicleMeshes(gfx.device, vehs[showcaseVehIndices[i]], showcaseModels[i]);
        }
        buildVehicleMeshes(gfx.device, vehs[4], escortModelA); // BVP M-80AB1
        buildVehicleMeshes(gfx.device, vehs[2], escortModelB); // Т-80БВМ
        buildEnvironmentAssets(gfx.device, env);

        checkLocalFiles();
        phase = PHASE_STARTUP_CHECK;
        phaseTimer = 0.f;
        displayPct = 0.f;
        return true;
    }

    void checkLocalFiles() {
        WIN32_FILE_ATTRIBUTE_DATA fad{}, fadVfs{};
        if (::GetFileAttributesExW(L"bin\\win64\\SteelFrontGame.exe", GetFileExInfoStandard, &fad) &&
            ::GetFileAttributesExW(L"game\\res\\gamedata.vromfs.bin", GetFileExInfoStandard, &fadVfs)) {
            ULONGLONG sz    = (ULONGLONG(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
            ULONGLONG vfsSz = (ULONGLONG(fadVfs.nFileSizeHigh) << 32) | fadVfs.nFileSizeLow;
            // Новый бинарник с настоящей Quirrel VM + Dagor Math весит > 1.0 МБ (старый v2.4 был ~540 КБ),
            // поэтому лаунчер автоматически обновит старый .exe в существующей папке!
            clientPresent = (sz > 1000 * 1024 && vfsSz > 4096);
        } else {
            clientPresent = false;
        }
    }

    static bool downloadFileHttps(const wchar_t* host, const wchar_t* urlPath, const wchar_t* destPath,
                                  std::atomic<int>& pctOut, int pctBase, int pctSpan, DWORD minBytes = 128) {
        ::CreateDirectoryW(L"bin", nullptr);
        ::CreateDirectoryW(L"bin\\win64", nullptr);
        ::CreateDirectoryW(L"game", nullptr);
        ::CreateDirectoryW(L"game\\res", nullptr);

        HINTERNET hSession = ::WinHttpOpen(L"SteelFrontDagorLauncher/6.5 (Windows NT 10.0; Win64; x64)",
                                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        HINTERNET hConnect = ::WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { ::WinHttpCloseHandle(hSession); return false; }

        HINTERNET hRequest = ::WinHttpOpenRequest(hConnect, L"GET", urlPath, nullptr,
                                                  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                  WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            return false;
        }

        bool ok = false;
        if (::WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            ::WinHttpReceiveResponse(hRequest, nullptr)) {

            DWORD statusCode = 0, scSize = sizeof(statusCode);
            ::WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                  WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &scSize, WINHTTP_NO_HEADER_INDEX);

            DWORD contentLen = 0, clSize = sizeof(contentLen);
            ::WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                  WINHTTP_HEADER_NAME_BY_INDEX, &contentLen, &clSize, WINHTTP_NO_HEADER_INDEX);

            if (statusCode == 200) {
                std::wstring tmpPath = std::wstring(destPath) + L".part";
                HANDLE hFile = ::CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hFile != INVALID_HANDLE_VALUE) {
                    uint8_t buf[32768];
                    DWORD totalRead = 0;
                    ok = true;
                    while (true) {
                        DWORD avail = 0;
                        if (!::WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) break;
                        DWORD toRead = std::min<DWORD>(avail, sizeof(buf));
                        DWORD bytesRead = 0;
                        if (!::WinHttpReadData(hRequest, buf, toRead, &bytesRead) || bytesRead == 0) break;
                        DWORD written = 0;
                        if (!::WriteFile(hFile, buf, bytesRead, &written, nullptr) || written != bytesRead) {
                            ok = false; break;
                        }
                        totalRead += bytesRead;
                        if (contentLen > 0) {
                            pctOut = pctBase + (int)((uint64_t)totalRead * pctSpan / contentLen);
                        }
                    }
                    ::CloseHandle(hFile);
                    if (ok && totalRead >= minBytes) {
                        ::MoveFileExW(tmpPath.c_str(), destPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
                    } else {
                        ::DeleteFileW(tmpPath.c_str());
                        ok = false;
                    }
                }
            }
        }

        ::WinHttpCloseHandle(hRequest);
        ::WinHttpCloseHandle(hConnect);
        ::WinHttpCloseHandle(hSession);
        return ok;
    }

    void startAsyncCdnDownload(bool forceUpdate) {
        if (netBusy.load()) return;
        netBusy = true;
        netPct = 4;
        netStage = 0;
        phase = PHASE_DOWNLOADING;

        std::thread([this, forceUpdate]() {
            checkLocalFiles();
            bool okGame = clientPresent;
            if (!clientPresent || forceUpdate) {
                netStage = 1;
                downloadFileHttps(kGithubHost, kAppBlkUrlPath,  L"application.blk", netPct, 2, 5, 128);
                downloadFileHttps(kGithubHost, kVromfsUrlPath,  L"game\\res\\gamedata.vromfs.bin", netPct, 7, 10, 4096);
                downloadFileHttps(kGithubHost, kSetupPsUrlPath, L"SETUP_FULL_DAGOR_ENGINE.ps1", netPct, 17, 5, 256);
                okGame = downloadFileHttps(kGithubHost, kGameUrlPath, L"bin\\win64\\SteelFrontGame.exe", netPct, 22, 56, 65536);
                netStage = 2;
                downloadFileHttps(kGithubHost, kServerUrlPath, L"bin\\win64\\SteelFrontServer.exe", netPct, 78, 20, 65536);
            }
            checkLocalFiles();
            netOk = (okGame || clientPresent);
            netPct = 100;
            netBusy = false;
        }).detach();
    }

    void beginLaunchSequence() {
        if (phase != PHASE_READY) return;
        checkLocalFiles();
        if (dagorEngineMode == 0 && !clientPresent) {
            startAsyncCdnDownload(true);
            return;
        }
        cfg.save();
        activeEditField = 0;
        phase = PHASE_LAUNCHING_GAME;
        phaseTimer = 0.f;
        displayPct = 0.f;
    }

    void executeSpawnGameProcess() {
        if (dagorEngineMode == 1 || dagorEngineMode == 2) {
            const wchar_t* psArgs = (dagorEngineMode == 1)
                ? L"-ExecutionPolicy Bypass -File \"SETUP_FULL_DAGOR_ENGINE.ps1\" -Mode dng-prebuilt"
                : L"-ExecutionPolicy Bypass -File \"SETUP_FULL_DAGOR_ENGINE.ps1\" -Mode full-sdk";
            ::ShellExecuteW(hwnd, L"open", L"powershell.exe", psArgs, nullptr, SW_SHOWNORMAL);
            phase = PHASE_READY;
            displayPct = 100.f;
            return;
        }

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        wchar_t cmd[] = L"\"bin\\win64\\SteelFrontGame.exe\"";
        if (::CreateProcessW(L"bin\\win64\\SteelFrontGame.exe", cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            ::CloseHandle(pi.hThread);
            ::CloseHandle(pi.hProcess);
            running = false;
            ::PostQuitMessage(0);
        } else {
            phase = PHASE_READY;
            displayPct = 100.f;
        }
    }

    void startDedicatedServer() {
        cfg.save();
        wchar_t params[128];
        std::swprintf(params, 128, L"--port %d --map 0", cfg.serverPort);
        ::ShellExecuteW(hwnd, L"open", L"bin\\win64\\SteelFrontServer.exe", params, nullptr, SW_SHOWNORMAL);
    }

    void update(float dt) {
        appTime += dt;
        if (!dragging3D && phase != PHASE_LAUNCHING_GAME) {
            camYaw = wrapAngle(camYaw + dt * 0.11f);
        }

        if (phase == PHASE_STARTUP_CHECK) {
            // Плавная многоэтапная проверка лаунчера при открытии (~4.1 секунды)
            const float totalStartupSec = 4.1f;
            phaseTimer += dt;
            float t = clampf(phaseTimer / totalStartupSec, 0.f, 1.f);
            // Небольшая реалистичная ступенчатость на проверке шейдеров и архивов
            float shaped = t < 0.45f ? (t * 1.05f) : (0.4725f + (t - 0.45f) * 0.959f);
            displayPct = clampf(shaped * 100.f, 0.f, 100.f);

            if (phaseTimer >= 2.4f && !clientPresent && !netBusy.load()) {
                // Если клиент ещё не скачан локально — плавно переходим к загрузке с GitHub CDN
                startAsyncCdnDownload(false);
                return;
            }
            if (phaseTimer >= totalStartupSec) {
                displayPct = 100.f;
                phase = PHASE_READY;
            }
        }
        else if (phase == PHASE_DOWNLOADING) {
            displayPct = lerpf(displayPct, (float)netPct.load(), std::min(1.f, dt * 8.f));
            if (!netBusy.load()) {
                displayPct = 100.f;
                phase = PHASE_READY;
            }
        }
        else if (phase == PHASE_LAUNCHING_GAME) {
            // Кинематографичный предстартовый цикл перед открытием игры (~2.9 секунды)
            const float totalLaunchSec = 2.9f;
            phaseTimer += dt;
            camDist = lerpf(camDist, 6.15f, std::min(1.f, dt * 2.6f));
            camPitch = lerpf(camPitch, 0.14f, std::min(1.f, dt * 2.4f));
            displayPct = clampf((phaseTimer / totalLaunchSec) * 100.f, 0.f, 100.f);
            if (phaseTimer >= totalLaunchSec) {
                executeSpawnGameProcess();
            }
        }
    }

    void drawShowcaseTank(const VehicleGpuModel& mdl, int vehIdx, const Vec3& pos, float hYaw, float tYaw, int camoIdx) {
        const VehicleSpec& vs = getVehicles()[vehIdx];
        const auto& camos = getCamouflages();
        const CamoSpec& cm = camos[camoIdx % camos.size()];
        Vec3 colA = cm.useVehicleBase ? vs.baseColor : cm.c0;

        CBObject obj{};
        obj.tintAlpha       = Vec4(colA.x, colA.y, colA.z, 1.f);
        obj.camoColBPattern = Vec4(cm.c1.x, cm.c1.y, cm.c1.z, (float)cm.idx);
        obj.camoColCObjMode = Vec4(cm.c2.x, cm.c2.y, cm.c2.z, 0.f);
        obj.armorParams     = Vec4((float)vs.hullArmor[0], (float)vs.hullArmor[1], (float)vs.hullArmor[2], 0.f);

        obj.world = Mat4::rotationY(hYaw) * Mat4::translation(pos.x, pos.y, pos.z);
        gfx.drawMesh(mdl.hull, obj);

        Mat4 turW = Mat4::rotationY(tYaw) * Mat4::translation(pos.x, pos.y, pos.z);
        obj.world = turW;
        gfx.drawMesh(mdl.turret, obj);

        const float pivotY = (vs.arch == ARCH_IFV) ? 1.56f : 1.54f;
        obj.world = Mat4::rotationX(0.045f) * Mat4::translation(0.f, pivotY, -0.45f) * turW;
        gfx.drawMesh(mdl.gun, obj);
    }

    void render3DShowcase() {
        float aspect = float(gfx.width) / std::max(1, gfx.height);
        Vec3 target(0.f, 1.32f, 0.f);
        Vec3 camPos(
            target.x + std::sin(camYaw) * std::cos(camPitch) * camDist,
            std::max(0.65f, target.y + std::sin(camPitch) * camDist),
            target.z + std::cos(camYaw) * std::cos(camPitch) * camDist
        );

        Vec3 sunDir = Vec3(0.58f, 0.56f, -0.59f).normalized();
        Mat4 lightVP = Mat4::lookAtLH(sunDir * 42.f, Vec3(0, 0.8f, 0), Vec3(0, 1, 0)) *
                       Mat4::orthoLH(26.f, 26.f, 4.f, 88.f);
        Mat4 viewProj = Mat4::lookAtLH(camPos, target, Vec3(0, 1, 0)) *
                        Mat4::perspectiveFovLH(deg2rad(43.f), aspect, 0.25f, 760.f);

        CBFrame fb{};
        fb.viewProj         = viewProj;
        fb.lightViewProj    = lightVP;
        fb.camPosTime       = Vec4(camPos.x, camPos.y, camPos.z, (float)appTime);
        fb.sunDirQuality    = Vec4(sunDir.x, sunDir.y, sunDir.z, 1.f);
        fb.sunColorExposure = Vec4(4.85f, 4.62f, 4.25f, 1.14f);
        fb.skyTopFogStart   = Vec4(0.12f, 0.34f, 0.71f, 75.f);
        fb.skyHorizFogEnd   = Vec4(0.60f, 0.75f, 0.90f, 255.f);
        fb.groundAmbMode    = Vec4(0.16f, 0.17f, 0.18f, 0.f);

        int vIdx = showcaseVehIndices[showcasePos];
        float turSweep = std::sin((float)appTime * 0.42f) * 0.22f;

        auto drawScene = [&](bool shadowPass) {
            if (!shadowPass) {
                CBObject sky{};
                sky.world = Mat4::translation(camPos.x, 0.f, camPos.z);
                sky.tintAlpha = Vec4(1, 1, 1, 1);
                gfx.drawMesh(env.skyDome, sky);
            }
            CBObject envObj{};
            envObj.world = Mat4::identity();
            envObj.tintAlpha = Vec4(1, 1, 1, 1);
            gfx.drawMesh(env.hangarOutdoor, envObj);

            drawShowcaseTank(escortModelA, 4, Vec3(-9.4f, 0.f, 8.6f),  0.46f,  0.30f, 1);
            drawShowcaseTank(escortModelB, 2, Vec3( 9.6f, 0.f, 9.0f), -0.44f, -0.26f, 1);
            drawShowcaseTank(showcaseModels[showcasePos], vIdx, Vec3(0.f, 0.f, 0.f), 0.f, turSweep, showcaseCamo);
        };

        gfx.beginShadowPass(fb);
        drawScene(true);

        gfx.beginMainPass(fb, Vec4(0.22f, 0.38f, 0.62f, 1.f));
        drawScene(false);
    }

    void addHit(int id, int param, float x, float y, float w, float h) {
        hitBoxes.push_back({id, param, x, y, w, h});
    }

    bool isHover(float x, float y, float w, float h) const {
        return mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h;
    }

    void getStatusStrings(const char*& outHeader, std::string& outDetail) const {
        char buf[256];
        if (phase == PHASE_STARTUP_CHECK) {
            outHeader = "ПРОВЕРКА ЦЕЛОСТНОСТИ И ИНИЦИАЛИЗАЦИЯ DAGOR DX11";
            if (displayPct < 18.f)
                outDetail = "Инициализация шейдерного конвейера DirectX 11 (Shader Model 5.0, 4x MSAA)...";
            else if (displayPct < 38.f)
                outDetail = "Проверка геометрии и PBR-материалов бронетехники (7 наций • 23 боевые машины)...";
            else if (displayPct < 58.f)
                outDetail = "Сверка таблиц баллистики БОПС / ПТУР и пакетов ДЗ «ERO-19 / Реликт»...";
            else if (displayPct < 78.f)
                outDetail = "Верификация исполняемых модулей bin\\win64\\SteelFrontGame.exe и конфигурации...";
            else if (displayPct < 95.f)
                outDetail = "Синхронизация профиля командира и проверка моста Steamworks SDK...";
            else
                outDetail = "Контрольные суммы SHA-256 подтверждены • Все модули актуальны!";
        }
        else if (phase == PHASE_DOWNLOADING) {
            outHeader = "ЗАГРУЗКА ОБНОВЛЕНИЯ КЛИЕНТА С GITHUB CDN";
            std::snprintf(buf, sizeof(buf), "%s (%d%%)",
                          netStage.load() <= 1 ? "Скачивание bin\\win64\\SteelFrontGame.exe по защищённому каналу HTTPS..."
                                               : "Скачивание выделенного сервера bin\\win64\\SteelFrontServer.exe...",
                          (int)displayPct);
            outDetail = buf;
        }
        else if (phase == PHASE_LAUNCHING_GAME) {
            outHeader = "ЗАПУСК БОЕВОГО КЛИЕНТА STEEL FRONT: ONLINE";
            if (displayPct < 30.f)
                outDetail = "Сохранение настроек графики, параметров экрана и сетевого адреса...";
            else if (displayPct < 65.f)
                outDetail = "Прогрев кэша шейдеров DirectX 11 и звукового синтезатора WinMM...";
            else if (displayPct < 90.f)
                outDetail = "Открытие сетевого сокета UDP и загрузка военной базы ангара...";
            else
                outDetail = "Передача управления в SteelFrontGame.exe — удачной охоты, командир!";
        }
        else {
            if (dagorEngineMode == 1) {
                outHeader = "РЕЖИМ 2: ОФИЦИАЛЬНЫЙ ПАКЕТ GAIJIN daNetGame (CDN PREBUILT)";
                outDetail = "Нажмите «РАЗВЕРНУТЬ», чтобы скачать dngSceneViewer.7z с CDN Gaijin и смонтировать модуль Steel Front (.blk + .das)";
            } else if (dagorEngineMode == 2) {
                outHeader = "РЕЖИМ 3: ПОЛНЫЙ ИСХОДНЫЙ SDK GAIJIN DAGOR ENGINE (GIT + JAM)";
                outDetail = "Нажмите «СОБРАТЬ SDK» для клонирования GaijinEntertainment/DagorEngine, установки devtools и сборки jam (~200 ГБ)";
            } else {
                const auto& dk = dagor::RuntimeKernel::instance();
                outHeader = clientPresent ? "РЕЖИМ 1: ЯДРО DAGOR ENGINE 6.5 (.BLK + VROMFS) ГОТОВО К БОЮ" : "ТРЕБУЕТСЯ ЗАГРУЗКА ПАКЕТА DAGOR 6.5";
                std::snprintf(buf, sizeof(buf),
                              "Dagor 6.5 (%d .BLK машин • %d daECS шаблонов • VROMFS OK)  •  Шлюз %s:%d",
                              dk.loadedVehiclesBlk, dk.entityMgr.templateCount(), cfg.serverIp, cfg.serverPort);
                outDetail = buf;
            }
        }
    }

    void renderLauncherUi() {
        hitBoxes.clear();
        ui.begin();
        const float fW = (float)gfx.width;
        const float fH = (float)gfx.height;
        const auto& vehs = getVehicles();
        const VehicleSpec& curV = vehs[showcaseVehIndices[showcasePos]];
        char buf[200];

        // Мягкое кинематографичное затемнение по краям (чтобы 3D-танк в центре сиял, а карточки читались идеально)
        ui.fillGradV(0, 0, fW, 88.f, Color4::rgba(6, 10, 16, 0.92f), Color4::rgba(6, 10, 16, 0.0f));
        ui.fillGradV(0, fH - 165.f, fW, 165.f, Color4::rgba(6, 10, 16, 0.0f), Color4::rgba(6, 9, 14, 0.96f));

        // =====================================================================
        // 1. ВЕРХНЯЯ ШАПКА ЛАУНЧЕРА GAIJIN daRg (CHARCOAL + AMBER)
        // =====================================================================
        ui.drawPanel(0, 0, fW, 52.f, Color4::rgba(16, 17, 19, 0.95f), Color4::rgba(10, 11, 13, 0.98f), Color4::rgba(228, 166, 46, 0.72f));
        ui.fillRect(0, 0, fW, 2.f, Color4::rgba(236, 174, 48, 0.98f));

        // Оранжево-золотой шеврон-эмблема Gaijin Dagor + заголовок
        ui.fillRect(20.f, 11.f, 5.f, 30.f, Color4::rgba(238, 86, 32));
        ui.drawText("STEEL FRONT // GAIJIN DAGOR 6.5", 34.f, 8.f, 19.5f, Color4::rgba(250, 246, 236));
        ui.drawText("QUIRREL VM (.NUT)  •  daSkies2  •  PERLIN LANDMESH  •  VROMFS", 34.f, 30.f, 10.8f, Color4::rgba(238, 182, 62));

        // Полоска флагов всех 7 наций в шапке
        for (int n = 0; n < 7; ++n) {
            ui.drawNationFlag(n, 418.f + n * 30.f, 17.f, 24.f, 16.f);
        }

        // Кнопки утилит в верхней панели
        auto topBtn = [&](float bx, float bw, const char* txt, int actId) {
            bool hov = isHover(bx, 11.f, bw, 30.f);
            ui.drawPanel(bx, 11.f, bw, 30.f,
                         hov ? Color4::rgba(68, 48, 16, 0.96f) : Color4::rgba(26, 27, 30, 0.94f),
                         Color4::rgba(16, 17, 19, 0.98f),
                         hov ? Color4::rgba(248, 192, 68) : Color4::rgba(92, 94, 102));
            ui.drawTextCentered(txt, bx + bw * 0.5f, 18.f, 11.6f, Color4::rgba(244, 238, 224));
            addHit(actId, 0, bx, 11.f, bw, 30.f);
        };
        topBtn(638.f, 138.f, "ОБНОВИТЬ С CDN", LACT_VERIFY_FILES);
        topBtn(784.f, 148.f, "ВЫДЕЛЕННЫЙ СЕРВЕР", LACT_START_SERVER);
        topBtn(940.f, 116.f, "CDK .BLK ТАНКОВ", LACT_OPEN_DAGOR_DIR);

        // Кнопки свернуть / закрыть окно
        bool hovMin = isHover(fW - 88.f, 11.f, 34.f, 30.f);
        ui.drawPanel(fW - 88.f, 11.f, 34.f, 30.f,
                     hovMin ? Color4::rgba(44, 56, 74) : Color4::rgba(22, 28, 38),
                     Color4::rgba(15, 19, 27), Color4::rgba(74, 90, 112));
        ui.drawTextCentered("—", fW - 71.f, 17.f, 14.f, Color4::rgba(225, 232, 242));
        addHit(LACT_MINIMIZE, 0, fW - 88.f, 11.f, 34.f, 30.f);

        bool hovCls = isHover(fW - 48.f, 11.f, 34.f, 30.f);
        ui.drawPanel(fW - 48.f, 11.f, 34.f, 30.f,
                     hovCls ? Color4::rgba(212, 48, 42) : Color4::rgba(92, 26, 26),
                     hovCls ? Color4::rgba(154, 24, 20) : Color4::rgba(58, 16, 16),
                     Color4::rgba(235, 102, 96));
        ui.drawTextCentered("✕", fW - 31.f, 17.f, 13.5f, Color4(1, 1, 1, 1));
        addHit(LACT_CLOSE, 0, fW - 48.f, 11.f, 34.f, 30.f);

        // =====================================================================
        // 2. ПЛАВАЮЩИЙ ПЕРЕКЛЮЧАТЕЛЬ 3D-ВИТРИНЫ ТАНКОВ НАД СЦЕНОЙ
        // =====================================================================
        float pillW = 396.f, pillX = fW * 0.5f - pillW * 0.5f - 10.f, pillY = 64.f;
        ui.drawPanel(pillX, pillY, pillW, 38.f, Color4::rgba(10, 15, 22, 0.84f), Color4::rgba(7, 11, 17, 0.90f), Color4::rgba(224, 178, 72, 0.65f));

        bool hovPrev = isHover(pillX + 6.f, pillY + 5.f, 32.f, 28.f);
        ui.drawPanel(pillX + 6.f, pillY + 5.f, 32.f, 28.f, hovPrev ? Color4::rgba(52, 74, 102) : Color4::rgba(22, 30, 42),
                     Color4::rgba(14, 20, 28), Color4::rgba(90, 114, 144));
        ui.drawTextCentered("◀", pillX + 22.f, pillY + 10.f, 13.f, Color4(1, 1, 1, 1));
        addHit(LACT_PREV_SHOWCASE, 0, pillX + 6.f, pillY + 5.f, 32.f, 28.f);

        ui.drawNationFlag(curV.nationIdx, pillX + 48.f, pillY + 10.f, 26.f, 17.f);
        std::snprintf(buf, sizeof(buf), "%s  •  БР %s", curV.shortName, curV.br);
        ui.drawTextCentered(buf, pillX + pillW * 0.5f + 8.f, pillY + 10.f, 13.5f, Color4::rgba(246, 232, 188));
        addHit(LACT_CYCLE_CAMO, 0, pillX + 44.f, pillY + 4.f, pillW - 88.f, 30.f);

        bool hovNext = isHover(pillX + pillW - 38.f, pillY + 5.f, 32.f, 28.f);
        ui.drawPanel(pillX + pillW - 38.f, pillY + 5.f, 32.f, 28.f, hovNext ? Color4::rgba(52, 74, 102) : Color4::rgba(22, 30, 42),
                     Color4::rgba(14, 20, 28), Color4::rgba(90, 114, 144));
        ui.drawTextCentered("▶", pillX + pillW - 22.f, pillY + 10.f, 13.f, Color4(1, 1, 1, 1));
        addHit(LACT_NEXT_SHOWCASE, 0, pillX + pillW - 38.f, pillY + 5.f, 32.f, 28.f);

        // =====================================================================
        // 3. ЛЕВАЯ КОЛОНКА — НОВОСТИ И ПАТЧНОУТ GAIJIN DAGOR ENGINE 6.5
        // =====================================================================
        float lx = 22.f, lw = 352.f;

        // Карточка 1: Переход на движок Gaijin Dagor Engine 6.5 + Балканский Щит
        ui.drawPanel(lx, 68.f, lw, 242.f, Color4::rgba(16, 17, 19, 0.91f), Color4::rgba(10, 11, 13, 0.96f), Color4::rgba(218, 162, 48, 0.72f));
        ui.fillRect(lx, 68.f, lw, 3.f, Color4::rgba(238, 176, 48));
        ui.fillRect(lx + 16.f, 84.f, 168.f, 20.f, Color4::rgba(212, 58, 26, 0.95f));
        ui.drawTextCentered("ДВИЖОК DAGOR ENGINE 6.5", lx + 100.f, 86.f, 10.6f, Color4(1, 1, 1, 1));
        ui.drawNationFlag(1, lx + lw - 48.f, 84.f, 32.f, 20.f);

        ui.drawText("ПЕРЕХОД НА GAIJIN DAGOR 6.5", lx + 16.f, 112.f, 16.6f, Color4::rgba(250, 246, 236));
        ui.drawText("Настоящие C++ модули DagorEngine + ветка ВС Сербии:", lx + 16.f, 138.f, 11.8f, Color4::rgba(238, 186, 76));
        ui.drawText("• ВМ Quirrel (.nut) + daSkies2 (4 погоды + облака и звёзды)", lx + 16.f, 160.f, 11.4f, Color4::rgba(218, 220, 224));
        ui.drawText("• Рельеф Perlin HeightMapLand + торсионная подвеска vehiclePhys", lx + 16.f, 180.f, 11.4f, Color4::rgba(218, 220, 224));
        ui.drawText("• M-84AS2 «Војвода», M-91 Vihor, M-84AS1, BVP M-80AB1", lx + 16.f, 200.f, 11.4f, Color4::rgba(218, 220, 224));
        ui.fillRect(lx + 16.f, 228.f, lw - 32.f, 1.f, Color4(1, 0.85f, 0.45f, 0.16f));
        ui.drawText("В игре: [~] Консоль Quirrel VM  •  [F3] Сетка  •  [F4] Погода", lx + 16.f, 240.f, 11.2f, Color4::rgba(148, 238, 162));
        ui.drawText("7 наций • 23 боевые машины • калькулятор брони под мышью!", lx + 16.f, 258.f, 11.2f, Color4::rgba(182, 184, 190));
        ui.drawText("Нажмите по плашке танка сверху для смены камуфляжа", lx + 16.f, 284.f, 11.0f, Color4::rgba(244, 202, 92));

        // Карточка 2: Выбор из 3 вариантов развёртывания Gaijin Dagor Engine 6.5
        ui.drawPanel(lx, 324.f, lw, 248.f, Color4::rgba(16, 17, 19, 0.92f), Color4::rgba(10, 11, 13, 0.96f), Color4::rgba(218, 162, 48, 0.72f));
        ui.fillRect(lx, 324.f, lw, 3.f, Color4::rgba(238, 176, 48));
        ui.drawText("АРХИТЕКТУРА DAGOR ENGINE 6.5 (3 РЕЖИМА)", lx + 14.f, 338.f, 13.4f, Color4::rgba(246, 198, 82));

        const char* mTitles[3] = {
            "1. DAGOR 6.5 RT (Quirrel VM + daSkies2)",
            "2. GAIJIN daNetGame (CDN PREBUILT x64)",
            "3. ПОЛНЫЙ DAGOR SDK (GIT + JAM • 200 ГБ)"
        };
        const char* mDescs[3] = {
            "Мгновенный старт! Quirrel .nut, VROMFS, Perlin LandMesh и F5",
            "Скачивает официальный dngSceneViewer.7z с CDN Gaijin + мод",
            "Клонирует GaijinEntertainment/DagorEngine + компиляция jam"
        };
        for (int m = 0; m < 3; ++m) {
            float my = 364.f + m * 64.f;
            bool sel = (dagorEngineMode == m);
            bool hov = isHover(lx + 12.f, my, lw - 24.f, 56.f);
            ui.drawPanel(lx + 12.f, my, lw - 24.f, 56.f,
                         sel ? Color4::rgba(78, 54, 16, 0.96f) : (hov ? Color4::rgba(38, 34, 26, 0.94f) : Color4::rgba(22, 23, 26, 0.92f)),
                         sel ? Color4::rgba(46, 30, 10, 0.98f) : Color4::rgba(14, 15, 17, 0.95f),
                         sel ? Color4::rgba(252, 196, 68) : Color4::rgba(74, 76, 82));
            if (sel) ui.fillRect(lx + 12.f, my, 4.f, 56.f, Color4::rgba(252, 196, 68));
            ui.drawText(mTitles[m], lx + 24.f, my + 8.f, 12.0f, sel ? Color4::rgba(255, 232, 152) : Color4::rgba(236, 234, 228));
            ui.drawText(mDescs[m], lx + 24.f, my + 30.f, 10.6f, sel ? Color4::rgba(248, 212, 128) : Color4::rgba(168, 170, 176));
            addHit(LACT_SELECT_DAGOR_MODE, m, lx + 12.f, my, lw - 24.f, 56.f);
        }

        // =====================================================================
        // 4. ПРАВАЯ КОЛОНКА — НАСТРОЙКИ КОМАНДИРА, СЕТИ И ГРАФИКИ
        // =====================================================================
        float rx = fW - 382.f, rw = 360.f, ry = 68.f;
        ui.drawPanel(rx, ry, rw, 504.f, Color4::rgba(16, 17, 19, 0.92f), Color4::rgba(10, 11, 13, 0.96f), Color4::rgba(218, 162, 48, 0.72f));
        ui.fillRect(rx, ry, rw, 3.f, Color4::rgba(238, 176, 48));
        ui.drawText("ПАРАМЕТРЫ КЛИЕНТА DAGOR 6.5", rx + 18.f, ry + 14.f, 15.2f, Color4::rgba(250, 246, 236));

        bool blink = (std::fmod(appTime, 1.0) < 0.52);

        // Поле: Позывной командира
        ui.drawText("ПОЗЫВНОЙ В ОНЛАЙН-БОЮ (НАЖМИТЕ ДЛЯ ВВОДА):", rx + 18.f, ry + 44.f, 11.2f, Color4::rgba(164, 182, 204));
        bool edCall = (activeEditField == 1);
        ui.drawPanel(rx + 18.f, ry + 62.f, rw - 36.f, 34.f,
                     edCall ? Color4::rgba(28, 44, 66) : Color4::rgba(17, 23, 33),
                     Color4::rgba(12, 17, 25),
                     edCall ? Color4::rgba(245, 196, 78) : Color4::rgba(68, 86, 110));
        std::snprintf(buf, sizeof(buf), "%s%s", cfg.callsign, (edCall && blink) ? "|" : "");
        ui.drawText(buf, rx + 28.f, ry + 70.f, 14.0f, Color4::rgba(244, 248, 255));
        addHit(LACT_EDIT_CALLSIGN, 0, rx + 18.f, ry + 62.f, rw - 36.f, 34.f);

        // Поле: IP сервера + быстрая кнопка 127.0.0.1
        ui.drawText("АДРЕС БОЕВОГО СЕРВЕРА UDP (IPv4):", rx + 18.f, ry + 108.f, 11.2f, Color4::rgba(164, 182, 204));
        bool edIp = (activeEditField == 2);
        ui.drawPanel(rx + 18.f, ry + 126.f, rw - 134.f, 34.f,
                     edIp ? Color4::rgba(28, 44, 66) : Color4::rgba(17, 23, 33),
                     Color4::rgba(12, 17, 25),
                     edIp ? Color4::rgba(245, 196, 78) : Color4::rgba(68, 86, 110));
        std::snprintf(buf, sizeof(buf), "%s:%d%s", cfg.serverIp, cfg.serverPort, (edIp && blink) ? "|" : "");
        ui.drawText(buf, rx + 28.f, ry + 134.f, 13.5f, Color4::rgba(136, 228, 164));
        addHit(LACT_EDIT_SERVER_IP, 0, rx + 18.f, ry + 126.f, rw - 134.f, 34.f);

        bool hovLoc = isHover(rx + rw - 108.f, ry + 126.f, 90.f, 34.f);
        ui.drawPanel(rx + rw - 108.f, ry + 126.f, 90.f, 34.f,
                     hovLoc ? Color4::rgba(42, 66, 94) : Color4::rgba(22, 32, 46),
                     Color4::rgba(15, 22, 32), Color4::rgba(86, 114, 146));
        ui.drawTextCentered("ЛОКАЛЬНЫЙ", rx + rw - 63.f, ry + 135.f, 11.2f, Color4::rgba(222, 234, 248));
        addHit(LACT_RESET_LOCAL_IP, 0, rx + rw - 108.f, ry + 126.f, 90.f, 34.f);

        // Пресет качества графики
        ui.drawText("ПРЕСЕТ КАЧЕСТВА ГРАФИКИ DIRECTX 11:", rx + 18.f, ry + 174.f, 11.2f, Color4::rgba(164, 182, 204));
        const char* qNames[4] = {"НИЗКИЕ", "СРЕДНИЕ", "ВЫСОКИЕ", "КИНО"};
        for (int q = 0; q < 4; ++q) {
            float qx = rx + 18.f + q * 82.f;
            bool sel = (cfg.qualityPreset == q);
            bool hov = isHover(qx, ry + 194.f, 76.f, 32.f);
            ui.drawPanel(qx, ry + 194.f, 76.f, 32.f,
                         sel ? Color4::rgba(198, 142, 32) : (hov ? Color4::rgba(36, 48, 66) : Color4::rgba(20, 26, 36)),
                         sel ? Color4::rgba(142, 96, 16)  : Color4::rgba(14, 18, 26),
                         sel ? Color4::rgba(255, 218, 112): Color4::rgba(66, 82, 104));
            ui.drawTextCentered(qNames[q], qx + 38.f, ry + 202.f, 11.6f, Color4(1, 1, 1, 1));
            addHit(LACT_QUALITY, q, qx, ry + 194.f, 76.f, 32.f);
        }

        // Режим окна
        ui.drawText("РЕЖИМ ОТОБРАЖЕНИЯ ЭКРАНА:", rx + 18.f, ry + 240.f, 11.2f, Color4::rgba(164, 182, 204));
        const char* dNames[3] = {"БЕЗ РАМКИ", "1600×900", "1280×720"};
        for (int d = 0; d < 3; ++d) {
            float dx = rx + 18.f + d * 110.f;
            bool sel = (cfg.displayMode == d);
            bool hov = isHover(dx, ry + 260.f, 104.f, 32.f);
            ui.drawPanel(dx, ry + 260.f, 104.f, 32.f,
                         sel ? Color4::rgba(46, 86, 128) : (hov ? Color4::rgba(34, 46, 64) : Color4::rgba(20, 26, 36)),
                         sel ? Color4::rgba(28, 54, 84)  : Color4::rgba(14, 18, 26),
                         sel ? Color4::rgba(122, 198, 255): Color4::rgba(66, 82, 104));
            ui.drawTextCentered(dNames[d], dx + 52.f, ry + 268.f, 11.6f, Color4(1, 1, 1, 1));
            addHit(LACT_DISPLAY, d, dx, ry + 260.f, 104.f, 32.f);
        }

        // Тумблеры опций рендера и звука
        auto drawToggle = [&](float ty, const char* label, bool on, int actId) {
            bool hov = isHover(rx + 18.f, ty, rw - 36.f, 36.f);
            ui.drawPanel(rx + 18.f, ty, rw - 36.f, 36.f,
                         hov ? Color4::rgba(26, 36, 50, 0.95f) : Color4::rgba(18, 24, 34, 0.92f),
                         Color4::rgba(13, 18, 26, 0.95f),
                         on ? Color4::rgba(84, 158, 116, 0.85f) : Color4::rgba(62, 76, 96, 0.7f));
            ui.drawText(label, rx + 30.f, ty + 9.f, 12.4f, Color4::rgba(226, 234, 246));

            float pillRx = rx + rw - 84.f;
            ui.drawPanel(pillRx, ty + 7.f, 54.f, 22.f,
                         on ? Color4::rgba(38, 142, 76) : Color4::rgba(54, 60, 72),
                         on ? Color4::rgba(24, 98, 50)  : Color4::rgba(36, 40, 50),
                         on ? Color4::rgba(138, 238, 168): Color4::rgba(96, 106, 122));
            ui.drawTextCentered(on ? "ВКЛ" : "ВЫКЛ", pillRx + 27.f, ty + 10.f, 10.8f, Color4(1, 1, 1, 1));
            addHit(actId, 0, rx + 18.f, ty, rw - 36.f, 36.f);
        };

        drawToggle(ry + 308.f, "Аппаратное сглаживание 4x MSAA",  cfg.msaaSamples > 1, LACT_TOGGLE_MSAA);
        drawToggle(ry + 352.f, "Мягкие каскадные тени PCF 2048²", cfg.shadows,         LACT_TOGGLE_SHADOWS);
        drawToggle(ry + 396.f, "Вертикальная синхронизация VSync", cfg.vsync,          LACT_TOGGLE_VSYNC);
        drawToggle(ry + 440.f, "Боевой аудио-синтезатор выстрелов", cfg.soundEnabled,  LACT_TOGGLE_SOUND);

        // =====================================================================
        // 5. НИЖНИЙ ДОК ПРОГРЕССА И КНОПКА «ИГРАТЬ» (В СТИЛЕ GAIJIN LAUNCHER)
        // =====================================================================
        float dockH = 104.f, dockY = fH - dockH;
        ui.drawPanel(0, dockY, fW, dockH, Color4::rgba(9, 13, 19, 0.95f), Color4::rgba(6, 9, 14, 0.98f), Color4::rgba(68, 86, 112, 0.65f));
        ui.fillRect(0, dockY, fW, 1.5f, Color4::rgba(228, 174, 58, 0.55f));

        const char* stHead = "";
        std::string stDetail;
        getStatusStrings(stHead, stDetail);

        Color4 headCol = (phase == PHASE_READY) ? Color4::rgba(126, 232, 158)
                       : (phase == PHASE_LAUNCHING_GAME ? Color4::rgba(255, 206, 96) : Color4::rgba(132, 204, 255));
        ui.drawText(stHead, 26.f, dockY + 15.f, 13.8f, headCol);

        char pctTxt[32];
        std::snprintf(pctTxt, sizeof(pctTxt), "%d%%", (int)std::round(displayPct));
        ui.drawTextRight(pctTxt, fW - 298.f, dockY + 14.f, 15.0f, Color4::rgba(244, 224, 156));

        ui.drawText(stDetail.c_str(), 26.f, dockY + 36.f, 12.0f, Color4::rgba(186, 202, 222));

        // Многослойная анимированная полоса прогресса
        float barX = 26.f, barY = dockY + 64.f, barW = fW - 324.f, barH = 20.f;
        ui.drawPanel(barX, barY, barW, barH, Color4::rgba(16, 22, 32), Color4::rgba(11, 15, 22), Color4::rgba(64, 82, 106));

        float fillW = std::max(0.f, (barW - 4.f) * clampf(displayPct / 100.f, 0.f, 1.f));
        if (fillW > 1.f) {
            Color4 cTop = (phase == PHASE_READY)          ? Color4::rgba(46, 182, 98)
                        : (phase == PHASE_LAUNCHING_GAME) ? Color4::rgba(238, 92, 48)
                                                          : Color4::rgba(58, 148, 236);
            Color4 cBot = (phase == PHASE_READY)          ? Color4::rgba(26, 122, 62)
                        : (phase == PHASE_LAUNCHING_GAME) ? Color4::rgba(176, 44, 24)
                                                          : Color4::rgba(32, 92, 168);
            ui.fillGradV(barX + 2.f, barY + 2.f, fillW, barH - 4.f, cTop, cBot);

            // Бегущий световой блик по полосе во время проверки / запуска
            if (phase != PHASE_READY && fillW > 48.f) {
                float pulsePos = std::fmod((float)appTime * 360.f, fillW);
                float pw = std::min(64.f, fillW - pulsePos);
                ui.fillRect(barX + 2.f + pulsePos, barY + 2.f, pw, barH - 4.f, Color4(1.f, 1.f, 1.f, 0.24f));
            }
        }

        // Главная кнопка «ИГРАТЬ»
        float playW = 256.f, playH = 68.f;
        float playX = fW - playW - 22.f, playY = dockY + 18.f;
        bool canPlay = (phase == PHASE_READY);
        bool hovPlay = canPlay && isHover(playX, playY, playW, playH);

        Color4 pTop = !canPlay ? Color4::rgba(46, 56, 72)
                    : (hovPlay ? Color4::rgba(242, 68, 48) : Color4::rgba(208, 42, 32));
        Color4 pBot = !canPlay ? Color4::rgba(30, 38, 50)
                    : (hovPlay ? Color4::rgba(182, 28, 20) : Color4::rgba(144, 20, 16));
        Color4 pBrd = !canPlay ? Color4::rgba(92, 112, 138) : Color4::rgba(255, 186, 118);

        ui.drawPanel(playX, playY, playW, playH, pTop, pBot, pBrd);
        const char* readyLbl = (dagorEngineMode == 1) ? "РАЗВЕРНУТЬ"
                             : (dagorEngineMode == 2) ? "СОБРАТЬ SDK"
                             : (clientPresent ? "ИГРАТЬ" : "СКАЧАТЬ");
        const char* readySub = (dagorEngineMode == 1) ? "GAIJIN CDN  •  daNetGame x64"
                             : (dagorEngineMode == 2) ? "DAGOR ENGINE  •  GIT + JAM"
                             : "DAGOR 6.5 RT  •  В АНГАР";
        const char* playLbl = (phase == PHASE_STARTUP_CHECK)  ? "ПРОВЕРКА..."
                            : (phase == PHASE_DOWNLOADING)    ? "ЗАГРУЗКА..."
                            : (phase == PHASE_LAUNCHING_GAME) ? "ЗАПУСК..."
                            : readyLbl;
        ui.drawTextCentered(playLbl, playX + playW * 0.5f, playY + 14.f, 24.f,
                            canPlay ? Color4(1, 1, 1, 1) : Color4::rgba(196, 208, 224));
        ui.drawTextCentered(canPlay ? readySub : "ПОЖАЛУЙСТА, ПОДОЖДИТЕ",
                            playX + playW * 0.5f, playY + 46.f, 10.8f,
                            canPlay ? Color4::rgba(255, 224, 168) : Color4::rgba(148, 164, 186));
        addHit(LACT_PLAY, 0, playX, playY, playW, playH);

        ui.flush(gfx.width, gfx.height);
    }

    bool handleUiClick(float mx, float my) {
        activeEditField = 0;
        for (int i = (int)hitBoxes.size() - 1; i >= 0; --i) {
            if (!hitBoxes[i].contains(mx, my)) continue;
            const auto& b = hitBoxes[i];
            switch (b.id) {
                case LACT_CLOSE:
                    cfg.save();
                    running = false;
                    ::PostQuitMessage(0);
                    return true;
                case LACT_MINIMIZE:
                    ::ShowWindow(hwnd, SW_MINIMIZE);
                    return true;
                case LACT_QUALITY:
                    cfg.qualityPreset = b.param;
                    cfg.shadows = (b.param >= 1);
                    cfg.msaaSamples = (b.param >= 2) ? 4 : 1;
                    cfg.save();
                    return true;
                case LACT_DISPLAY:
                    cfg.displayMode = b.param;
                    cfg.save();
                    return true;
                case LACT_TOGGLE_MSAA:
                    cfg.msaaSamples = (cfg.msaaSamples > 1) ? 1 : 4;
                    cfg.save();
                    return true;
                case LACT_TOGGLE_SHADOWS:
                    cfg.shadows = !cfg.shadows;
                    cfg.save();
                    return true;
                case LACT_TOGGLE_VSYNC:
                    cfg.vsync = !cfg.vsync;
                    cfg.save();
                    return true;
                case LACT_TOGGLE_SOUND:
                    cfg.soundEnabled = !cfg.soundEnabled;
                    cfg.save();
                    return true;
                case LACT_EDIT_CALLSIGN:
                    activeEditField = 1;
                    return true;
                case LACT_EDIT_SERVER_IP:
                    activeEditField = 2;
                    return true;
                case LACT_RESET_LOCAL_IP:
                    std::snprintf(cfg.serverIp, sizeof(cfg.serverIp), "127.0.0.1");
                    cfg.save();
                    return true;
                case LACT_VERIFY_FILES:
                    if (phase == PHASE_READY) {
                        initOrReloadDagorGamedata(true);
                        const auto& vehs = getVehicles();
                        for (size_t k = 0; k < showcaseVehIndices.size(); ++k) {
                            buildVehicleMeshes(gfx.device, vehs[showcaseVehIndices[k]], showcaseModels[k]);
                        }
                        checkLocalFiles();
                        if (!clientPresent) {
                            startAsyncCdnDownload(true);
                        } else {
                            phase = PHASE_STARTUP_CHECK;
                            phaseTimer = 0.f;
                            displayPct = 0.f;
                        }
                    }
                    return true;
                case LACT_START_SERVER:
                    startDedicatedServer();
                    return true;
                case LACT_OPEN_DAGOR_DIR: {
                    std::string blkDisk;
                    if (dagor::RuntimeKernel::instance().vfs.ensureEditableBlkOnDisk("gamedata/units/tanks_all_nations.blk", blkDisk)) {
                        for (char& c : blkDisk) if (c == '/') c = '\\';
                        ::ShellExecuteA(hwnd, "open", "notepad.exe", blkDisk.c_str(), nullptr, SW_SHOWNORMAL);
                    } else {
                        ::ShellExecuteW(hwnd, L"explore", L"prog", nullptr, nullptr, SW_SHOWNORMAL);
                    }
                    return true;
                }
                case LACT_SELECT_DAGOR_MODE:
                    dagorEngineMode = std::clamp(b.param, 0, 2);
                    return true;
                case LACT_PREV_SHOWCASE:
                    showcasePos = (showcasePos + (int)showcaseVehIndices.size() - 1) % (int)showcaseVehIndices.size();
                    return true;
                case LACT_NEXT_SHOWCASE:
                    showcasePos = (showcasePos + 1) % (int)showcaseVehIndices.size();
                    return true;
                case LACT_CYCLE_CAMO:
                    showcaseCamo = (showcaseCamo + 1) % (int)getCamouflages().size();
                    return true;
                case LACT_PLAY:
                    beginLaunchSequence();
                    return true;
            }
        }
        return false;
    }

    void onCharInput(wchar_t wc) {
        if (activeEditField == 0) {
            if (wc == L'\r') beginLaunchSequence();
            return;
        }
        char* target = (activeEditField == 1) ? cfg.callsign : cfg.serverIp;
        size_t maxLen = (activeEditField == 1) ? 24 : 42;
        size_t len = std::strlen(target);

        if (wc == L'\b') {
            while (len > 0) {
                uint8_t c = (uint8_t)target[--len];
                target[len] = 0;
                if ((c & 0xC0) != 0x80) break;
            }
        } else if (wc == L'\r' || wc == L'\x1B') {
            activeEditField = 0;
            cfg.save();
        } else if (wc >= 32) {
            if (activeEditField == 2 && !((wc >= L'0' && wc <= L'9') || wc == L'.')) return;
            char utf8[8] = {};
            int n = ::WideCharToMultiByte(CP_UTF8, 0, &wc, 1, utf8, sizeof(utf8), nullptr, nullptr);
            if (len + n < maxLen) {
                std::memcpy(target + len, utf8, n);
                target[len + n] = 0;
            }
        }
    }

    void shutdown() {
        for (auto& m : showcaseModels) m.release();
        escortModelA.release();
        escortModelB.release();
        env.release();
        ui.shutdown();
        gfx.shutdown();
    }
};

static D3DLauncherApp gLauncher;

static LRESULT CALLBACK LauncherWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_MOUSEMOVE:
            gLauncher.mouseX = (float)GET_X_LPARAM(lParam);
            gLauncher.mouseY = (float)GET_Y_LPARAM(lParam);
            if (gLauncher.dragging3D) {
                int dx = GET_X_LPARAM(lParam) - gLauncher.lastDragPt.x;
                int dy = GET_Y_LPARAM(lParam) - gLauncher.lastDragPt.y;
                gLauncher.camYaw   = wrapAngle(gLauncher.camYaw + dx * 0.0065f);
                gLauncher.camPitch = clampf(gLauncher.camPitch + dy * 0.005f, 0.06f, 0.62f);
                gLauncher.lastDragPt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            }
            return 0;
        case WM_LBUTTONDOWN: {
            float mx = (float)GET_X_LPARAM(lParam);
            float my = (float)GET_Y_LPARAM(lParam);
            gLauncher.mouseX = mx;
            gLauncher.mouseY = my;
            if (gLauncher.handleUiClick(mx, my)) return 0;

            // Перетаскивание окна за свободную зону верхней шапки
            if (my < 52.f) {
                ::ReleaseCapture();
                ::SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }

            // Вращение 3D-танка прямо в фоне лаунчера!
            gLauncher.dragging3D = true;
            gLauncher.lastDragPt = {(LONG)mx, (LONG)my};
            ::SetCapture(hWnd);
            return 0;
        }
        case WM_RBUTTONDOWN:
            gLauncher.dragging3D = true;
            gLauncher.lastDragPt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ::SetCapture(hWnd);
            return 0;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
            if (gLauncher.dragging3D) {
                gLauncher.dragging3D = false;
                ::ReleaseCapture();
            }
            return 0;
        case WM_MOUSEWHEEL: {
            float steps = float(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            gLauncher.camDist = clampf(gLauncher.camDist - steps * 0.55f, 5.4f, 12.0f);
            return 0;
        }
        case WM_CHAR:
            gLauncher.onCharInput((wchar_t)wParam);
            return 0;
        case WM_DESTROY:
            gLauncher.running = false;
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    ::SetProcessDPIAware();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = LauncherWndProc;
    wc.hInstance = hInst;
    wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SteelFrontDX11Launcher";
    ::RegisterClassExW(&wc);

    int sx = (::GetSystemMetrics(SM_CXSCREEN) - D3DLauncherApp::W) / 2;
    int sy = (::GetSystemMetrics(SM_CYSCREEN) - D3DLauncherApp::H) / 2;
    HWND hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"Steel Front: Online — Лаунчер",
                                  WS_POPUP | WS_VISIBLE,
                                  std::max(0, sx), std::max(0, sy),
                                  D3DLauncherApp::W, D3DLauncherApp::H,
                                  nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;

    BOOL dark = TRUE;
    ::DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    DWORD roundPref = 2; /*DWMWCP_ROUND (Windows 11 скруглённые углы окна)*/
    ::DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &roundPref, sizeof(roundPref));

    if (!gLauncher.init(hwnd)) {
        ::MessageBoxW(hwnd, L"Не удалось инициализировать DirectX 11 для лаунчера!", L"Steel Front Launcher", MB_ICONERROR);
        return 1;
    }

    LARGE_INTEGER freq{}, prev{};
    ::QueryPerformanceFrequency(&freq);
    ::QueryPerformanceCounter(&prev);

    MSG msg{};
    while (gLauncher.running) {
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) gLauncher.running = false;
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
        if (!gLauncher.running) break;

        LARGE_INTEGER now{};
        ::QueryPerformanceCounter(&now);
        float dt = float(double(now.QuadPart - prev.QuadPart) / double(freq.QuadPart));
        prev = now;
        dt = clampf(dt, 0.0005f, 0.08f);

        gLauncher.update(dt);
        if (!gLauncher.running) break;

        gLauncher.render3DShowcase();
        gLauncher.renderLauncherUi();
        gLauncher.gfx.present();
    }

    gLauncher.shutdown();
    return 0;
}
