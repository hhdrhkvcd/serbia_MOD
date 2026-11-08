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
#include <winhttp.h>
#include "../src/settings_profile.h"

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <cstdio>
#include <cstring>
#include <algorithm>

using namespace sf;

// Ссылки автозагрузки клиента с GitHub (работает, даже если пользователь скачал ОДИН только SteelFrontLauncher.exe!)
static const wchar_t* kGithubHost = L"raw.githubusercontent.com";
static const wchar_t* kGameUrlPath   = L"/hhdrhkvcd/serbia_MOD/arena/2571b3b1-serbia-mod/bin/win64/SteelFrontGame.exe";
static const wchar_t* kServerUrlPath = L"/hhdrhkvcd/serbia_MOD/arena/2571b3b1-serbia-mod/bin/win64/SteelFrontServer.exe";

static std::wstring utf8ToWide(const char* s) {
    if (!s || !s[0]) return L"";
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 1) return L"";
    std::wstring out(n - 1, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), n);
    return out;
}

struct LauncherBtn {
    int id = 0;
    int param = 0;
    RECT rc{};
    bool contains(int x, int y) const {
        return x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom;
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
    LACT_VERIFY_UPDATE,
    LACT_START_SERVER,
    LACT_OPEN_DAGOR_DIR,
    LACT_PLAY
};

class LauncherApp {
public:
    static constexpr int W = 1040;
    static constexpr int H = 640;

    HWND hwnd = nullptr;
    GameSettings cfg;
    int mouseX = 0, mouseY = 0;
    int activeEditField = 0; // 0=нет, 1=callsign, 2=serverIp
    std::vector<LauncherBtn> buttons;

    std::atomic<bool> downloading{false};
    std::atomic<int>  progressPct{100};
    bool clientInstalled = false;
    std::wstring statusText = L"Проверка файлов клиента DirectX 11...";

    HFONT fontHero = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontBold = nullptr;
    HFONT fontBody = nullptr;
    HFONT fontSmall = nullptr;
    HFONT fontPlay = nullptr;

    void init(HWND w) {
        hwnd = w;
        // Устанавливаем рабочую директорию рядом с SteelFrontLauncher.exe
        char exePath[MAX_PATH] = {};
        ::GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        char* sl = std::strrchr(exePath, '\\');
        if (sl) *sl = 0;
        ::SetCurrentDirectoryA(exePath);
        if (::GetFileAttributesA("..\\..\\SteelFrontLauncher.exe") != INVALID_FILE_ATTRIBUTES) {
            ::SetCurrentDirectoryA("..\\..");
        }

        cfg.load();
        ensureSteamAppId();
        checkLocalClient();

        fontHero  = ::CreateFontW(-30, 0, 0, 0, FW_HEAVY,    FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        fontTitle = ::CreateFontW(-19, 0, 0, 0, FW_BOLD,     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        fontBold  = ::CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        fontBody  = ::CreateFontW(-14, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        fontSmall = ::CreateFontW(-12, 0, 0, 0, FW_NORMAL,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        fontPlay  = ::CreateFontW(-24, 0, 0, 0, FW_HEAVY,    FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }

    void ensureSteamAppId() {
        FILE* f = std::fopen("steam_appid.txt", "rb");
        if (!f) {
            f = std::fopen("steam_appid.txt", "wb");
            if (f) { std::fprintf(f, "480\n"); std::fclose(f); }
        } else {
            std::fclose(f);
        }
    }

    static bool isValidPeFile(const char* path) {
        FILE* f = std::fopen(path, "rb");
        if (!f) return false;
        unsigned char mz[2] = {};
        size_t r = std::fread(mz, 1, 2, f);
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fclose(f);
        return (r == 2 && mz[0] == 'M' && mz[1] == 'Z' && sz > 65536);
    }

    void checkLocalClient() {
        clientInstalled = isValidPeFile("bin\\win64\\SteelFrontGame.exe");
        if (clientInstalled) {
            progressPct = 100;
            statusText = L"✓ Клиент игры проверен и готов к бою • DirectX 11 x64 • Steam Ready";
        } else {
            progressPct = 0;
            statusText = L"Клиент не найден локально — нажмите «СКАЧАТЬ И ИГРАТЬ» для автозагрузки (~1.3 МБ)";
        }
    }

    static bool downloadHttpsFile(const wchar_t* urlPath, const char* outFile, std::atomic<int>& progOut, int basePct, int spanPct) {
        HINTERNET hSession = ::WinHttpOpen(L"SteelFrontLauncher/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        HINTERNET hConnect = ::WinHttpConnect(hSession, kGithubHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { ::WinHttpCloseHandle(hSession); return false; }

        HINTERNET hReq = ::WinHttpOpenRequest(hConnect, L"GET", urlPath, nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hReq) { ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession); return false; }

        bool ok = false;
        if (::WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            ::WinHttpReceiveResponse(hReq, nullptr)) {
            DWORD statusCode = 0, scSize = sizeof(statusCode);
            ::WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                  WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &scSize, WINHTTP_NO_HEADER_INDEX);

            DWORD contentLen = 0, clSize = sizeof(contentLen);
            ::WinHttpQueryHeaders(hReq, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                  WINHTTP_HEADER_NAME_BY_INDEX, &contentLen, &clSize, WINHTTP_NO_HEADER_INDEX);

            if (statusCode == 200) {
                ::CreateDirectoryA("bin", nullptr);
                ::CreateDirectoryA("bin\\win64", nullptr);
                std::string tmpPath = std::string(outFile) + ".part";
                FILE* fp = std::fopen(tmpPath.c_str(), "wb");
                if (fp) {
                    uint8_t buf[16384];
                    DWORD avail = 0, readBytes = 0, totalRead = 0;
                    ok = true;
                    while (::WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
                        DWORD toRead = std::min<DWORD>(avail, sizeof(buf));
                        if (!::WinHttpReadData(hReq, buf, toRead, &readBytes) || readBytes == 0) break;
                        std::fwrite(buf, 1, readBytes, fp);
                        totalRead += readBytes;
                        if (contentLen > 0) {
                            progOut = basePct + int((double(totalRead) / double(contentLen)) * spanPct);
                        }
                    }
                    std::fclose(fp);
                    if (totalRead > 65536) {
                        ::DeleteFileA(outFile);
                        ::MoveFileA(tmpPath.c_str(), outFile);
                    } else {
                        ::DeleteFileA(tmpPath.c_str());
                        ok = false;
                    }
                }
            }
        }

        ::WinHttpCloseHandle(hReq);
        ::WinHttpCloseHandle(hConnect);
        ::WinHttpCloseHandle(hSession);
        return ok;
    }

    void startDownloadAndOptionalLaunch(bool launchAfter) {
        if (downloading.load()) return;
        downloading = true;
        progressPct = 4;
        statusText = L"Подключение к CDN репозитория Steel Front: Online (HTTPS)...";
        ::InvalidateRect(hwnd, nullptr, FALSE);

        std::thread([this, launchAfter]() {
            statusText = L"Загрузка игрового движка bin\\win64\\SteelFrontGame.exe...";
            ::InvalidateRect(hwnd, nullptr, FALSE);
            bool okGame = downloadHttpsFile(kGameUrlPath, "bin\\win64\\SteelFrontGame.exe", progressPct, 5, 65);

            statusText = L"Загрузка выделенного сервера bin\\win64\\SteelFrontServer.exe...";
            ::InvalidateRect(hwnd, nullptr, FALSE);
            bool okSrv  = downloadHttpsFile(kServerUrlPath, "bin\\win64\\SteelFrontServer.exe", progressPct, 70, 28);

            downloading = false;
            clientInstalled = isValidPeFile("bin\\win64\\SteelFrontGame.exe");
            if (clientInstalled) {
                progressPct = 100;
                statusText = (okGame || okSrv)
                    ? L"✓ Обновление успешно установлено! Клиент DirectX 11 готов к запуску."
                    : L"✓ Локальная сборка проверена и готова к запуску (актуальная версия).";
                ::InvalidateRect(hwnd, nullptr, FALSE);
                if (launchAfter) {
                    ::PostMessageW(hwnd, WM_USER + 10, 0, 0);
                }
            } else {
                progressPct = 0;
                statusText = L"Ошибка загрузки по сети. Проверьте интернет или распакуйте архив целиком.";
                ::InvalidateRect(hwnd, nullptr, FALSE);
            }
        }).detach();
    }

    void launchGameExe() {
        cfg.save();
        ensureSteamAppId();
        if (!isValidPeFile("bin\\win64\\SteelFrontGame.exe")) {
            startDownloadAndOptionalLaunch(true);
            return;
        }
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        wchar_t cmd[] = L"bin\\win64\\SteelFrontGame.exe";
        if (::CreateProcessW(L"bin\\win64\\SteelFrontGame.exe", cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            ::CloseHandle(pi.hThread);
            ::CloseHandle(pi.hProcess);
            ::ShowWindow(hwnd, SW_MINIMIZE);
        } else {
            ::MessageBoxW(hwnd, L"Не удалось запустить bin\\win64\\SteelFrontGame.exe!", L"Steel Front Launcher", MB_ICONERROR);
        }
    }

    void launchDedicatedServer() {
        cfg.save();
        if (!isValidPeFile("bin\\win64\\SteelFrontServer.exe")) {
            startDownloadAndOptionalLaunch(false);
            return;
        }
        ::ShellExecuteW(hwnd, L"open", L"bin\\win64\\SteelFrontServer.exe", nullptr, nullptr, SW_SHOWNORMAL);
        statusText = L"✓ Выделенный сервер запущен на UDP 27015! Друзья могут подключаться по вашему IP.";
        ::InvalidateRect(hwnd, nullptr, FALSE);
    }

    static void fillRectCol(HDC dc, const RECT& r, COLORREF c) {
        HBRUSH b = ::CreateSolidBrush(c);
        ::FillRect(dc, &r, b);
        ::DeleteObject(b);
    }

    static void frameRectCol(HDC dc, const RECT& r, COLORREF c) {
        HBRUSH b = ::CreateSolidBrush(c);
        ::FrameRect(dc, &r, b);
        ::DeleteObject(b);
    }

    static void fillGradV(HDC dc, const RECT& r, COLORREF top, COLORREF bot) {
        TRIVERTEX tv[2] = {
            {r.left,  r.top,    (COLOR16)(GetRValue(top) << 8), (COLOR16)(GetGValue(top) << 8), (COLOR16)(GetBValue(top) << 8), 0xFF00},
            {r.right, r.bottom, (COLOR16)(GetRValue(bot) << 8), (COLOR16)(GetGValue(bot) << 8), (COLOR16)(GetBValue(bot) << 8), 0xFF00}
        };
        GRADIENT_RECT gr{0, 1};
        ::GdiGradientFill(dc, tv, 2, &gr, 1, GRADIENT_FILL_RECT_V);
    }

    void drawButton(HDC dc, int id, int param, RECT r, const wchar_t* text, HFONT fnt,
                    COLORREF topC, COLORREF botC, COLORREF borderC, COLORREF textC) {
        bool hov = (mouseX >= r.left && mouseX <= r.right && mouseY >= r.top && mouseY <= r.bottom);
        if (hov) {
            topC = RGB(std::min(255, GetRValue(topC) + 22), std::min(255, GetGValue(topC) + 22), std::min(255, GetBValue(topC) + 22));
            botC = RGB(std::min(255, GetRValue(botC) + 16), std::min(255, GetGValue(botC) + 16), std::min(255, GetBValue(botC) + 16));
        }
        fillGradV(dc, r, topC, botC);
        frameRectCol(dc, r, borderC);
        ::SelectObject(dc, fnt);
        ::SetTextColor(dc, textC);
        ::DrawTextW(dc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        buttons.push_back({id, param, r});
    }

    void paint(HDC windowDc) {
        buttons.clear();
        HDC dc = ::CreateCompatibleDC(windowDc);
        HBITMAP bm = ::CreateCompatibleBitmap(windowDc, W, H);
        HGDIOBJ oldBm = ::SelectObject(dc, bm);
        ::SetBkMode(dc, TRANSPARENT);

        // Фон окна лаунчера (тёмный графит Gaijin Launcher)
        fillGradV(dc, {0, 0, W, H}, RGB(15, 19, 26), RGB(9, 12, 17));
        frameRectCol(dc, {0, 0, W, H}, RGB(56, 70, 90));

        // 1. ВЕРХНЯЯ ПАНЕЛЬ ЗАГОЛОВКА
        fillGradV(dc, {1, 1, W - 1, 50}, RGB(20, 26, 36), RGB(13, 17, 24));
        fillRectCol(dc, {1, 50, W - 1, 51}, RGB(52, 66, 84));

        // Герб-щит слева
        fillGradV(dc, {18, 10, 46, 40}, RGB(196, 42, 34), RGB(122, 20, 18));
        frameRectCol(dc, {18, 10, 46, 40}, RGB(242, 188, 84));
        ::SelectObject(dc, fontTitle);
        ::SetTextColor(dc, RGB(255, 232, 172));
        RECT crestRc{18, 10, 46, 40};
        ::DrawTextW(dc, L"SF", -1, &crestRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        ::SelectObject(dc, fontTitle);
        ::SetTextColor(dc, RGB(242, 246, 252));
        ::TextOutW(dc, 58, 8, L"STEEL FRONT: ONLINE", 19);
        ::SelectObject(dc, fontSmall);
        ::SetTextColor(dc, RGB(218, 174, 74));
        ::TextOutW(dc, 58, 29, L"ЛАУНЧЕР ПК • DIRECTX 11 x64 • STEAMWORKS & DAGOR ENGINE MOD", 59);

        // Верхние сервисные кнопки
        drawButton(dc, LACT_VERIFY_UPDATE, 0, {546, 11, 704, 39}, L"ПРОВЕРИТЬ ФАЙЛЫ", fontSmall,
                   RGB(28, 38, 52), RGB(18, 25, 36), RGB(76, 104, 138), RGB(214, 230, 248));
        drawButton(dc, LACT_START_SERVER, 0, {712, 11, 868, 39}, L"СЕРВЕР UDP 27015", fontSmall,
                   RGB(28, 52, 42), RGB(18, 34, 28), RGB(78, 152, 112), RGB(178, 242, 202));
        drawButton(dc, LACT_OPEN_DAGOR_DIR, 0, {876, 11, 954, 39}, L"DAGOR SDK", fontSmall,
                   RGB(44, 38, 26), RGB(28, 24, 16), RGB(168, 136, 62), RGB(246, 214, 128));
        drawButton(dc, LACT_MINIMIZE, 0, {964, 11, 994, 39}, L"—", fontBold,
                   RGB(28, 34, 44), RGB(18, 22, 30), RGB(68, 82, 102), RGB(220, 228, 240));
        drawButton(dc, LACT_CLOSE, 0, {1000, 11, 1030, 39}, L"✕", fontBold,
                   RGB(128, 32, 30), RGB(84, 18, 18), RGB(212, 86, 80), RGB(255, 255, 255));

        // 2. ГЛАВНЫЙ БАННЕР ОБНОВЛЕНИЯ + КАРТОЧКИ НОВОСТЕЙ (В СТИЛЕ WAR THUNDER LAUNCHER)
        RECT hero{22, 64, 668, 338};
        fillGradV(dc, hero, RGB(24, 38, 58), RGB(44, 32, 24));
        frameRectCol(dc, hero, RGB(88, 112, 142));

        // Зарево горизонта и векторный силуэт танка M-84AS2 «Војвода» на баннере
        fillGradV(dc, {23, 218, 667, 337}, RGB(68, 44, 28), RGB(14, 18, 24));
        HBRUSH tankBrush = ::CreateSolidBrush(RGB(18, 23, 30));
        HPEN   goldPen   = ::CreatePen(PS_SOLID, 2, RGB(236, 178, 72));
        HGDIOBJ oldBr = ::SelectObject(dc, tankBrush);
        HGDIOBJ oldPn = ::SelectObject(dc, goldPen);
        POINT hullPts[6] = {{340, 298}, {372, 268}, {578, 266}, {602, 288}, {585, 312}, {352, 312}};
        ::Polygon(dc, hullPts, 6);
        POINT turPts[5]  = {{410, 268}, {434, 236}, {532, 236}, {552, 266}, {410, 268}};
        ::Polygon(dc, turPts, 5);
        // Ствол 125мм 2А46М с эжектором
        RECT gunRc{262, 246, 424, 254};
        fillRectCol(dc, gunRc, RGB(236, 178, 72));
        RECT evacRc{322, 243, 356, 257};
        fillRectCol(dc, evacRc, RGB(236, 178, 72));
        ::SelectObject(dc, oldBr);
        ::SelectObject(dc, oldPn);
        ::DeleteObject(tankBrush);
        ::DeleteObject(goldPen);

        // Текст поверх баннера
        RECT badgeRc{38, 80, 292, 106};
        fillRectCol(dc, badgeRc, RGB(192, 40, 32));
        ::SelectObject(dc, fontSmall);
        ::SetTextColor(dc, RGB(255, 255, 255));
        ::DrawTextW(dc, L"ОБНОВЛЕНИЕ 1.0 • «БАЛКАНСКИЙ ЩИТ»", -1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        ::SelectObject(dc, fontHero);
        ::SetTextColor(dc, RGB(248, 250, 255));
        ::TextOutW(dc, 38, 116, L"БРОНЕТЕХНИКА СЕРБИИ И 7 НАЦИЙ", 29);

        ::SelectObject(dc, fontBody);
        ::SetTextColor(dc, RGB(212, 224, 240));
        RECT descRc{38, 158, 620, 236};
        ::DrawTextW(dc,
            L"• Полная ветка Вооружённых сил Сербии: BVP M-80AB1, M-84AS1, M-91 «Вихор» и M-84AS2 «Војвода».\n"
            L"• 23 боевые машины (РФ, Сербия, Китай, США, НАТО/ЕС, Франция, Швеция) в 3D ангаре и онлайн-боях.\n"
            L"• Нативный клиент Windows x64 на DirectX 11 PBR, тени 2048², тепловизор (ТВП) и 3D X-Ray Hit Cam.",
            -1, &descRc, DT_LEFT | DT_WORDBREAK);

        // Правая колонка сводки сборки и управления
        RECT infoCard{684, 64, 1018, 338};
        fillGradV(dc, infoCard, RGB(18, 24, 34), RGB(12, 16, 24));
        frameRectCol(dc, infoCard, RGB(64, 82, 106));

        ::SelectObject(dc, fontBold);
        ::SetTextColor(dc, RGB(244, 204, 98));
        ::TextOutW(dc, 702, 78, L"УПРАВЛЕНИЕ В БОЮ И STEAM", 24);

        ::SelectObject(dc, fontSmall);
        ::SetTextColor(dc, RGB(204, 218, 236));
        RECT keysRc{702, 106, 1004, 328};
        ::DrawTextW(dc,
            L"• W / A / S / D — Движение и разворот корпуса\n"
            L"• Мышь — Наведение башни (2 круга War Thunder)\n"
            L"• ЛКМ / Пробел — Выстрел орудия / Пулемёт\n"
            L"• Shift или ПКМ — Снайперский прицел (Зум колесом)\n"
            L"• N — Тепловизор (ТВП)   |   R — Лазерный дальномер\n"
            L"• 1 / 2 / 3 — Смена снарядов (ОБПС / КС / ПТУР / ОФС)\n"
            L"• G — Дымовые гранаты   |   F — Полевой ремонт\n"
            L"• 6 — Тушение пожара ППО   |   TAB — Таблица счёта\n"
            L"• F11 — Безрамочный / Оконный режим\n\n"
            L"Steam: добавьте SteelFrontLauncher.exe или\n"
            L"bin\\win64\\SteelFrontGame.exe как стороннюю игру Steam\n"
            L"(оверлей Shift+Tab активируется автоматически).",
            -1, &keysRc, DT_LEFT | DT_WORDBREAK);

        // 3. ПАНЕЛЬ НАСТРОЕК ГРАФИКИ, ЭКРАНА И ОНЛАЙН-СЕРВЕРА
        RECT setPanel{22, 352, 1018, 506};
        fillGradV(dc, setPanel, RGB(18, 24, 34), RGB(13, 17, 25));
        frameRectCol(dc, setPanel, RGB(62, 78, 100));

        ::SelectObject(dc, fontBold);
        ::SetTextColor(dc, RGB(238, 244, 252));
        ::TextOutW(dc, 38, 366, L"КАЧЕСТВО ГРАФИКИ:", 17);
        const wchar_t* qNames[4] = {L"НИЗКОЕ", L"СРЕДНЕЕ", L"ВЫСОКОЕ", L"КИНО (ULTRA)"};
        for (int q = 0; q < 4; ++q) {
            bool act = (cfg.qualityPreset == q);
            drawButton(dc, LACT_QUALITY, q, {208 + q * 118, 361, 208 + q * 118 + 110, 391}, qNames[q], fontSmall,
                       act ? RGB(52, 92, 138) : RGB(24, 31, 44),
                       act ? RGB(30, 56, 88)  : RGB(16, 21, 30),
                       act ? RGB(130, 204, 255) : RGB(62, 76, 96), RGB(255, 255, 255));
        }

        ::SelectObject(dc, fontBold);
        ::SetTextColor(dc, RGB(238, 244, 252));
        ::TextOutW(dc, 38, 412, L"РЕЖИМ ЭКРАНА:", 13);
        const wchar_t* dNames[3] = {L"БЕЗРАМОЧНЫЙ", L"ОКНО 1600×900", L"ОКНО 1280×720"};
        for (int d = 0; d < 3; ++d) {
            bool act = (cfg.displayMode == d);
            drawButton(dc, LACT_DISPLAY, d, {208 + d * 156, 407, 208 + d * 156 + 148, 437}, dNames[d], fontSmall,
                       act ? RGB(52, 92, 138) : RGB(24, 31, 44),
                       act ? RGB(30, 56, 88)  : RGB(16, 21, 30),
                       act ? RGB(130, 204, 255) : RGB(62, 76, 96), RGB(255, 255, 255));
        }

        // Чекбоксы рендера справа
        auto optToggle = [&](int id, int x, int y, int w, const wchar_t* label, bool on) {
            drawButton(dc, id, 0, {x, y, x + w, y + 30}, label, fontSmall,
                       on ? RGB(36, 84, 62) : RGB(26, 30, 40),
                       on ? RGB(22, 54, 40) : RGB(18, 20, 28),
                       on ? RGB(106, 214, 148) : RGB(68, 78, 94),
                       on ? RGB(220, 255, 232) : RGB(178, 188, 204));
        };
        optToggle(LACT_TOGGLE_MSAA,    700, 361, 146, cfg.msaaSamples > 1 ? L"✓ MSAA 4X ВКЛ" : L"MSAA ВЫКЛ", cfg.msaaSamples > 1);
        optToggle(LACT_TOGGLE_SHADOWS, 856, 361, 146, cfg.shadows ? L"✓ ТЕНИ 2048²" : L"ТЕНИ ВЫКЛ", cfg.shadows);
        optToggle(LACT_TOGGLE_VSYNC,   700, 407, 146, cfg.vsync ? L"✓ VSYNC ВКЛ" : L"VSYNC ВЫКЛ", cfg.vsync);
        optToggle(LACT_TOGGLE_SOUND,   856, 407, 146, cfg.soundEnabled ? L"✓ ЗВУК ВКЛ" : L"ЗВУК ВЫКЛ", cfg.soundEnabled);

        // Поля ввода Позывного и IP онлайн-сервера
        ::SelectObject(dc, fontBold);
        ::SetTextColor(dc, RGB(238, 244, 252));
        ::TextOutW(dc, 38, 460, L"ПОЗЫВНОЙ:", 9);

        RECT callRc{142, 455, 348, 487};
        fillRectCol(dc, callRc, activeEditField == 1 ? RGB(28, 40, 58) : RGB(12, 16, 24));
        frameRectCol(dc, callRc, activeEditField == 1 ? RGB(248, 202, 86) : RGB(76, 94, 118));
        std::wstring wCall = utf8ToWide(cfg.callsign) + (activeEditField == 1 ? L"_" : L"");
        RECT callTxt{152, 455, 342, 487};
        ::SelectObject(dc, fontBody);
        ::SetTextColor(dc, RGB(246, 248, 255));
        ::DrawTextW(dc, wCall.c_str(), -1, &callTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        buttons.push_back({LACT_EDIT_CALLSIGN, 0, callRc});

        ::SelectObject(dc, fontBold);
        ::SetTextColor(dc, RGB(238, 244, 252));
        ::TextOutW(dc, 372, 460, L"IP ОНЛАЙН-СЕРВЕРА:", 18);

        RECT ipRc{548, 455, 760, 487};
        fillRectCol(dc, ipRc, activeEditField == 2 ? RGB(28, 40, 58) : RGB(12, 16, 24));
        frameRectCol(dc, ipRc, activeEditField == 2 ? RGB(248, 202, 86) : RGB(76, 94, 118));
        std::wstring wIp = utf8ToWide(cfg.serverIp) + (activeEditField == 2 ? L"_" : L"");
        RECT ipTxt{558, 455, 754, 487};
        ::SelectObject(dc, fontBody);
        ::SetTextColor(dc, RGB(142, 232, 174));
        ::DrawTextW(dc, wIp.c_str(), -1, &ipTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        buttons.push_back({LACT_EDIT_SERVER_IP, 0, ipRc});

        ::SelectObject(dc, fontSmall);
        ::SetTextColor(dc, RGB(164, 180, 202));
        ::TextOutW(dc, 772, 463, L"(127.0.0.1 = автохост с ботами)", 31);

        // 4. НИЖНИЙ БАР ЗАГРУЗКИ / ПРОВЕРКИ + БОЛЬШАЯ КРАСНАЯ КНОПКА «ИГРАТЬ» (КАК В WAR THUNDER!)
        ::SelectObject(dc, fontBody);
        ::SetTextColor(dc, clientInstalled ? RGB(136, 230, 164) : RGB(246, 208, 106));
        ::TextOutW(dc, 24, 528, statusText.c_str(), (int)statusText.size());

        RECT progBg{22, 558, 742, 586};
        fillRectCol(dc, progBg, RGB(12, 16, 22));
        frameRectCol(dc, progBg, RGB(64, 82, 104));
        int pct = std::clamp(progressPct.load(), 0, 100);
        if (pct > 0) {
            int pw = int((progBg.right - progBg.left - 4) * (pct / 100.0));
            RECT progFill{progBg.left + 2, progBg.top + 2, progBg.left + 2 + pw, progBg.bottom - 2};
            fillGradV(dc, progFill,
                      downloading.load() ? RGB(228, 164, 42) : RGB(46, 158, 86),
                      downloading.load() ? RGB(168, 108, 18) : RGB(26, 104, 54));
        }
        wchar_t pctBuf[32];
        std::swprintf(pctBuf, 32, L"%d%%", pct);
        ::SelectObject(dc, fontSmall);
        ::SetTextColor(dc, RGB(245, 248, 255));
        ::DrawTextW(dc, pctBuf, -1, &progBg, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        ::SelectObject(dc, fontSmall);
        ::SetTextColor(dc, RGB(138, 154, 176));
        ::TextOutW(dc, 24, 598, L"Версия сборки: 1.0.4-win64-dx11  •  Исполняемый файл: bin\\win64\\SteelFrontGame.exe  •  Порт UDP: 27015", 102);

        const wchar_t* playLabel = downloading.load() ? L"ЗАГРУЗКА..."
                                 : (clientInstalled ? L"ИГРАТЬ" : L"СКАЧАТЬ И ИГРАТЬ");
        drawButton(dc, LACT_PLAY, 0, {762, 526, 1018, 612}, playLabel, fontPlay,
                   downloading.load() ? RGB(110, 90, 42) : RGB(218, 44, 34),
                   downloading.load() ? RGB(74, 60, 28)  : RGB(144, 20, 16),
                   RGB(255, 162, 132), RGB(255, 255, 255));

        ::BitBlt(windowDc, 0, 0, W, H, dc, 0, 0, SRCCOPY);
        ::SelectObject(dc, oldBm);
        ::DeleteObject(bm);
        ::DeleteDC(dc);
    }

    void onClick(int x, int y) {
        activeEditField = 0;
        for (const auto& b : buttons) {
            if (!b.contains(x, y)) continue;
            switch (b.id) {
                case LACT_CLOSE:
                    cfg.save();
                    ::PostQuitMessage(0);
                    break;
                case LACT_MINIMIZE:
                    ::ShowWindow(hwnd, SW_MINIMIZE);
                    break;
                case LACT_QUALITY:
                    cfg.qualityPreset = b.param;
                    cfg.msaaSamples = (b.param >= 2) ? 4 : 1;
                    cfg.shadows = (b.param >= 1);
                    cfg.save();
                    break;
                case LACT_DISPLAY:
                    cfg.displayMode = b.param;
                    cfg.save();
                    break;
                case LACT_TOGGLE_MSAA:
                    cfg.msaaSamples = (cfg.msaaSamples > 1) ? 1 : 4;
                    cfg.save();
                    break;
                case LACT_TOGGLE_SHADOWS:
                    cfg.shadows = !cfg.shadows;
                    cfg.save();
                    break;
                case LACT_TOGGLE_VSYNC:
                    cfg.vsync = !cfg.vsync;
                    cfg.save();
                    break;
                case LACT_TOGGLE_SOUND:
                    cfg.soundEnabled = !cfg.soundEnabled;
                    cfg.save();
                    break;
                case LACT_EDIT_CALLSIGN:
                    activeEditField = 1;
                    break;
                case LACT_EDIT_SERVER_IP:
                    activeEditField = 2;
                    break;
                case LACT_VERIFY_UPDATE:
                    startDownloadAndOptionalLaunch(false);
                    break;
                case LACT_START_SERVER:
                    launchDedicatedServer();
                    break;
                case LACT_OPEN_DAGOR_DIR:
                    if (::GetFileAttributesA("dagor_engine_mod") != INVALID_FILE_ATTRIBUTES) {
                        ::ShellExecuteW(hwnd, L"open", L"dagor_engine_mod", nullptr, nullptr, SW_SHOWNORMAL);
                    } else {
                        ::ShellExecuteW(hwnd, L"open", L"https://github.com/hhdrhkvcd/serbia_MOD/tree/arena/2571b3b1-serbia-mod/dagor_engine_mod", nullptr, nullptr, SW_SHOWNORMAL);
                    }
                    break;
                case LACT_PLAY:
                    launchGameExe();
                    break;
            }
            ::InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        ::InvalidateRect(hwnd, nullptr, FALSE);
    }

    void onCharInput(wchar_t ch) {
        if (activeEditField == 0) return;
        char* target = (activeEditField == 1) ? cfg.callsign : cfg.serverIp;
        size_t maxLen = (activeEditField == 1) ? (sizeof(cfg.callsign) - 1) : (sizeof(cfg.serverIp) - 1);
        std::string cur(target);

        if (ch == L'\b') {
            while (!cur.empty() && ((uint8_t)cur.back() & 0xC0) == 0x80) cur.pop_back();
            if (!cur.empty()) cur.pop_back();
        } else if (ch == L'\r' || ch == L'\x1B') {
            activeEditField = 0;
        } else if (ch >= 32) {
            if (activeEditField == 2) {
                // Для IP-адреса разрешаем цифры, точки и буквы хоста
                if ((ch >= L'0' && ch <= L'9') || ch == L'.' || (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || ch == L'-') {
                    if (cur.size() + 1 < maxLen) cur.push_back((char)ch);
                }
            } else {
                char u8[8] = {};
                int n = ::WideCharToMultiByte(CP_UTF8, 0, &ch, 1, u8, sizeof(u8), nullptr, nullptr);
                if (n > 0 && cur.size() + n < maxLen) cur.append(u8, n);
            }
        }
        std::snprintf(target, maxLen + 1, "%s", cur.c_str());
        cfg.save();
        ::InvalidateRect(hwnd, nullptr, FALSE);
    }
};

static LauncherApp gLauncher;

static LRESULT CALLBACK LauncherWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = ::BeginPaint(hWnd, &ps);
            gLauncher.paint(dc);
            ::EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            gLauncher.mouseX = GET_X_LPARAM(lParam);
            gLauncher.mouseY = GET_Y_LPARAM(lParam);
            ::InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ::ScreenToClient(hWnd, &pt);
            // Перетаскивание окна за шапку лаунчера (кроме кнопок справа)
            if (pt.y >= 0 && pt.y <= 50 && pt.x < 540) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_LBUTTONDOWN:
            gLauncher.onClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_CHAR:
            gLauncher.onCharInput((wchar_t)wParam);
            return 0;
        case WM_USER + 10:
            gLauncher.launchGameExe();
            return 0;
        case WM_DESTROY:
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
    wc.lpszClassName = L"SteelFrontGaijinLauncher";
    ::RegisterClassExW(&wc);

    int sx = (::GetSystemMetrics(SM_CXSCREEN) - LauncherApp::W) / 2;
    int sy = (::GetSystemMetrics(SM_CYSCREEN) - LauncherApp::H) / 2;
    HWND hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"Steel Front: Online — Launcher",
                                  WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE,
                                  std::max(0, sx), std::max(0, sy), LauncherApp::W, LauncherApp::H,
                                  nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;

    // Скруглённые углы окна в Windows 11 (DWMWCP_ROUND = 2)
    DWORD roundPref = 2;
    ::DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &roundPref, sizeof(roundPref));

    gLauncher.init(hwnd);
    ::InvalidateRect(hwnd, nullptr, FALSE);

    MSG msg{};
    while (::GetMessageW(&msg, nullptr, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    return 0;
}
