#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace sf {

enum SoundId {
    SND_CANNON = 0,
    SND_AUTOCANNON,
    SND_MG,
    SND_RICOCHET,
    SND_PENETRATION,
    SND_EXPLOSION,
    SND_RELOAD_READY,
    SND_UI_CLICK,
    SND_COUNT
};

class SoundSystem {
public:
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int CHANNELS = 8;

    HWAVEOUT hWave[CHANNELS] = {};
    WAVEHDR  hdrs[CHANNELS]  = {};
    bool     enabled = true;
    float    masterVol = 0.72f;
    std::vector<int16_t> clips[SND_COUNT];
    int nextChan = 0;

    void init() {
        WAVEFORMATEX wfx{};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 1;
        wfx.nSamplesPerSec = SAMPLE_RATE;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = 2;
        wfx.nAvgBytesPerSec = SAMPLE_RATE * 2;

        for (int i = 0; i < CHANNELS; ++i) {
            if (::waveOutOpen(&hWave[i], WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
                hWave[i] = nullptr;
            }
        }
        generateClips();
    }

    static float frand(uint32_t& seed) {
        seed = seed * 1664525u + 1013904223u;
        return (float((seed >> 8) & 0xFFFF) / 32767.5f) - 1.f;
    }

    void generateClips() {
        uint32_t seed = 0x9E3779B9u;

        // 1. Выстрел 120/125-мм танкового орудия (ударная волна + суббас + раскат)
        {
            int n = int(SAMPLE_RATE * 0.88f);
            clips[SND_CANNON].resize(n);
            float lp = 0.f;
            for (int i = 0; i < n; ++i) {
                float t = float(i) / SAMPLE_RATE;
                float punch = std::sin(2.f * 3.14159f * (128.f * std::exp(-t * 14.f) + 34.f) * t) * std::exp(-t * 8.5f);
                float noise = frand(seed) * std::exp(-t * 4.8f);
                lp += (noise - lp) * (0.38f * std::exp(-t * 2.2f) + 0.04f);
                float crack = frand(seed) * std::exp(-t * 48.f) * 0.9f;
                float s = std::tanh((punch * 1.15f + lp * 1.45f + crack) * 1.35f);
                clips[SND_CANNON][i] = (int16_t)(std::clamp(s * 0.85f, -1.f, 1.f) * 32760.f);
            }
        }
        // 2. 30-мм автопушка БМП M-80AB1
        {
            int n = int(SAMPLE_RATE * 0.28f);
            clips[SND_AUTOCANNON].resize(n);
            float lp = 0.f;
            for (int i = 0; i < n; ++i) {
                float t = float(i) / SAMPLE_RATE;
                float bark = std::sin(2.f * 3.14159f * (220.f * std::exp(-t * 22.f) + 65.f) * t) * std::exp(-t * 16.f);
                lp += (frand(seed) - lp) * 0.42f;
                float s = std::tanh((bark + lp * std::exp(-t * 14.f)) * 1.3f);
                clips[SND_AUTOCANNON][i] = (int16_t)(std::clamp(s * 0.72f, -1.f, 1.f) * 32760.f);
            }
        }
        // 3. Спаренный пулемёт ПКТ / M2HB
        {
            int n = int(SAMPLE_RATE * 0.11f);
            clips[SND_MG].resize(n);
            for (int i = 0; i < n; ++i) {
                float t = float(i) / SAMPLE_RATE;
                float s = (frand(seed) * 0.7f + std::sin(t * 950.f) * 0.35f) * std::exp(-t * 36.f);
                clips[SND_MG][i] = (int16_t)(std::clamp(s * 0.52f, -1.f, 1.f) * 32760.f);
            }
        }
        // 4. Металлический свист рикошета
        {
            int n = int(SAMPLE_RATE * 0.42f);
            clips[SND_RICOCHET].resize(n);
            float ph1 = 0.f, ph2 = 0.f;
            for (int i = 0; i < n; ++i) {
                float t = float(i) / SAMPLE_RATE;
                float f1 = 2350.f - t * 2900.f;
                float f2 = 3420.f - t * 3600.f;
                ph1 += 2.f * 3.14159f * std::max(300.f, f1) / SAMPLE_RATE;
                ph2 += 2.f * 3.14159f * std::max(400.f, f2) / SAMPLE_RATE;
                float ping = (std::sin(ph1) * 0.6f + std::sin(ph2) * 0.35f) * std::exp(-t * 6.5f);
                float clang = frand(seed) * std::exp(-t * 50.f) * 0.6f;
                clips[SND_RICOCHET][i] = (int16_t)(std::clamp((ping + clang) * 0.65f, -1.f, 1.f) * 32760.f);
            }
        }
        // 5. Пробитие брони (тяжёлый металлический удар + осколки)
        {
            int n = int(SAMPLE_RATE * 0.36f);
            clips[SND_PENETRATION].resize(n);
            float lp = 0.f;
            for (int i = 0; i < n; ++i) {
                float t = float(i) / SAMPLE_RATE;
                float thud = std::sin(2.f * 3.14159f * 92.f * t) * std::exp(-t * 14.f);
                float ring = std::sin(2.f * 3.14159f * 640.f * t) * std::exp(-t * 20.f) * 0.4f;
                lp += (frand(seed) - lp) * 0.55f;
                float s = std::tanh((thud + ring + lp * std::exp(-t * 12.f)) * 1.3f);
                clips[SND_PENETRATION][i] = (int16_t)(std::clamp(s * 0.78f, -1.f, 1.f) * 32760.f);
            }
        }
        // 6. Взрыв танка / детонация БК
        {
            int n = int(SAMPLE_RATE * 1.25f);
            clips[SND_EXPLOSION].resize(n);
            float lp = 0.f;
            for (int i = 0; i < n; ++i) {
                float t = float(i) / SAMPLE_RATE;
                lp += (frand(seed) - lp) * 0.14f;
                float boom = std::sin(2.f * 3.14159f * (68.f * std::exp(-t * 4.f) + 26.f) * t) * std::exp(-t * 3.5f);
                float s = std::tanh((boom * 1.2f + lp * 1.8f * std::exp(-t * 2.6f)) * 1.4f);
                clips[SND_EXPLOSION][i] = (int16_t)(std::clamp(s * 0.88f, -1.f, 1.f) * 32760.f);
            }
        }
        // 7. Затвор автомата заряжания готов
        {
            int n = int(SAMPLE_RATE * 0.18f);
            clips[SND_RELOAD_READY].resize(n);
            for (int i = 0; i < n; ++i) {
                float t = float(i) / SAMPLE_RATE;
                float clack1 = std::sin(t * 3200.f) * std::exp(-t * 45.f);
                float clack2 = (t > 0.065f) ? std::sin((t - 0.065f) * 4400.f) * std::exp(-(t - 0.065f) * 55.f) : 0.f;
                clips[SND_RELOAD_READY][i] = (int16_t)(std::clamp((clack1 + clack2) * 0.48f, -1.f, 1.f) * 32760.f);
            }
        }
        // 8. Щелчок интерфейса ангара
        {
            int n = int(SAMPLE_RATE * 0.045f);
            clips[SND_UI_CLICK].resize(n);
            for (int i = 0; i < n; ++i) {
                float t = float(i) / SAMPLE_RATE;
                float s = std::sin(2.f * 3.14159f * 1180.f * t) * std::exp(-t * 95.f);
                clips[SND_UI_CLICK][i] = (int16_t)(std::clamp(s * 0.32f, -1.f, 1.f) * 32760.f);
            }
        }
    }

    void play(SoundId id, float volScale = 1.f) {
        if (!enabled || id < 0 || id >= SND_COUNT || clips[id].empty()) return;
        int ch = nextChan;
        nextChan = (nextChan + 1) % CHANNELS;
        if (!hWave[ch]) return;

        ::waveOutReset(hWave[ch]);
        if (hdrs[ch].dwFlags & WHDR_PREPARED) {
            ::waveOutUnprepareHeader(hWave[ch], &hdrs[ch], sizeof(WAVEHDR));
        }
        DWORD vol = (DWORD)(std::clamp(masterVol * volScale, 0.f, 1.f) * 0xFFFF);
        ::waveOutSetVolume(hWave[ch], vol | (vol << 16));

        hdrs[ch] = WAVEHDR{};
        hdrs[ch].lpData = (LPSTR)clips[id].data();
        hdrs[ch].dwBufferLength = (DWORD)(clips[id].size() * sizeof(int16_t));
        if (::waveOutPrepareHeader(hWave[ch], &hdrs[ch], sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
            ::waveOutWrite(hWave[ch], &hdrs[ch], sizeof(WAVEHDR));
        }
    }

    void shutdown() {
        for (int i = 0; i < CHANNELS; ++i) {
            if (hWave[i]) {
                ::waveOutReset(hWave[i]);
                if (hdrs[i].dwFlags & WHDR_PREPARED) {
                    ::waveOutUnprepareHeader(hWave[i], &hdrs[i], sizeof(WAVEHDR));
                }
                ::waveOutClose(hWave[i]);
                hWave[i] = nullptr;
            }
        }
    }
};

} // namespace sf
