#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "math3d.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>

namespace sf {

struct Color4 {
    float r, g, b, a;
    constexpr Color4(float r_=1, float g_=1, float b_=1, float a_=1) : r(r_), g(g_), b(b_), a(a_) {}
    static Color4 rgba(int R, int G, int B, float A = 1.f) {
        return Color4(R / 255.f, G / 255.f, B / 255.f, A);
    }
};

struct UiVert {
    float x, y;
    float u, v;
    float r, g, b, a;
};

struct GlyphInfo {
    float u0, v0, u1, v1;
    float w, h;
    float advance;
};

static const char* kUiHlsl = R"HLSL(
cbuffer CBUi : register(b0) {
    float4 uScreenSize; // xy = width, height
};
Texture2D gAtlas : register(t0);
SamplerState gSamp : register(s0);

struct VSIn {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};
struct PSIn {
    float4 svPos : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 col   : COLOR0;
};
PSIn VSUi(VSIn vin) {
    PSIn o;
    float nx = (vin.pos.x / max(1.0, uScreenSize.x)) * 2.0 - 1.0;
    float ny = 1.0 - (vin.pos.y / max(1.0, uScreenSize.y)) * 2.0;
    o.svPos = float4(nx, ny, 0.0, 1.0);
    o.uv = vin.uv;
    o.col = vin.col;
    return o;
}
float4 PSUi(PSIn pin) : SV_Target {
    float4 tex = gAtlas.Sample(gSamp, pin.uv);
    return tex * pin.col;
}
)HLSL";

// Декодер UTF-8 -> Unicode codepoint (поддержка русского, сербского кириллического и спецсимволов)
inline uint32_t nextUtf8(const char*& p) {
    uint8_t c0 = (uint8_t)*p;
    if (!c0) return 0;
    ++p;
    if (c0 < 0x80) return c0;
    if ((c0 & 0xE0) == 0xC0 && (*p & 0xC0) == 0x80) {
        uint32_t cp = ((c0 & 0x1F) << 6) | ((uint8_t)*p++ & 0x3F);
        return cp;
    }
    if ((c0 & 0xF0) == 0xE0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) {
        uint32_t cp = ((c0 & 0x0F) << 12) | (((uint8_t)p[0] & 0x3F) << 6) | ((uint8_t)p[1] & 0x3F);
        p += 2;
        return cp;
    }
    if ((c0 & 0xF8) == 0xF0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        uint32_t cp = ((c0 & 0x07) << 18) | (((uint8_t)p[0] & 0x3F) << 12) | (((uint8_t)p[1] & 0x3F) << 6) | ((uint8_t)p[2] & 0x3F);
        p += 3;
        return cp;
    }
    return '?';
}

class UiRendererD3D11 {
public:
    static constexpr int ATLAS_W = 1024;
    static constexpr int ATLAS_H = 1024;
    static constexpr int BASE_FONT_PX = 28;
    static constexpr UINT MAX_UI_VERTS = 65536;

    ID3D11Device*             dev = nullptr;
    ID3D11DeviceContext*      ctx = nullptr;
    ID3D11VertexShader*       vs = nullptr;
    ID3D11PixelShader*        ps = nullptr;
    ID3D11InputLayout*        layout = nullptr;
    ID3D11Buffer*             vb = nullptr;
    ID3D11Buffer*             cb = nullptr;
    ID3D11ShaderResourceView* atlasSRV = nullptr;
    ID3D11SamplerState*       samp = nullptr;
    ID3D11DepthStencilState*  dssOff = nullptr;
    ID3D11BlendState*         bsAlpha = nullptr;
    ID3D11RasterizerState*    rsUi = nullptr;

    std::unordered_map<uint32_t, GlyphInfo> glyphs;
    float whiteU = 2.f / ATLAS_W;
    float whiteV = 2.f / ATLAS_H;
    std::vector<UiVert> batch;

    bool init(ID3D11Device* d, ID3D11DeviceContext* c) {
        dev = d; ctx = c;
        batch.reserve(MAX_UI_VERTS);

        ID3DBlob *vsb = nullptr, *psb = nullptr, *err = nullptr;
        if (FAILED(D3DCompile(kUiHlsl, std::strlen(kUiHlsl), "ui.hlsl", nullptr, nullptr, "VSUi", "vs_5_0", 0, 0, &vsb, &err))) {
            if (err) err->Release();
            return false;
        }
        if (FAILED(D3DCompile(kUiHlsl, std::strlen(kUiHlsl), "ui.hlsl", nullptr, nullptr, "PSUi", "ps_5_0", 0, 0, &psb, &err))) {
            if (err) err->Release();
            vsb->Release();
            return false;
        }
        dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &vs);
        dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &ps);

        D3D11_INPUT_ELEMENT_DESC ied[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        dev->CreateInputLayout(ied, 3, vsb->GetBufferPointer(), vsb->GetBufferSize(), &layout);
        vsb->Release(); psb->Release();

        D3D11_BUFFER_DESC vbd{};
        vbd.ByteWidth = MAX_UI_VERTS * sizeof(UiVert);
        vbd.Usage = D3D11_USAGE_DYNAMIC;
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        dev->CreateBuffer(&vbd, nullptr, &vb);

        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth = 16;
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        dev->CreateBuffer(&cbd, nullptr, &cb);

        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        dev->CreateSamplerState(&sd, &samp);

        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable = FALSE;
        dev->CreateDepthStencilState(&dsd, &dssOff);

        D3D11_BLEND_DESC bd{};
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        dev->CreateBlendState(&bd, &bsAlpha);

        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        dev->CreateRasterizerState(&rd, &rsUi);

        return buildFontAtlas();
    }

    bool buildFontAtlas() {
        HDC hdc = ::CreateCompatibleDC(nullptr);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = ATLAS_W;
        bmi.bmiHeader.biHeight = -ATLAS_H; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        uint32_t* dibPixels = nullptr;
        HBITMAP hbm = ::CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&dibPixels, nullptr, 0);
        if (!hbm || !dibPixels) { ::DeleteDC(hdc); return false; }

        HGDIOBJ oldBm = ::SelectObject(hdc, hbm);
        std::memset(dibPixels, 0, ATLAS_W * ATLAS_H * 4);

        HFONT hFont = ::CreateFontW(-BASE_FONT_PX, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HGDIOBJ oldFont = ::SelectObject(hdc, hFont);
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, RGB(255, 255, 255));

        // Собираем все кодовые точки: ASCII + Русская и Сербская кириллица + спецсимволы интерфейса
        std::vector<uint32_t> cps;
        for (uint32_t c = 0x20; c <= 0x7E; ++c) cps.push_back(c);
        for (uint32_t c = 0x0400; c <= 0x045F; ++c) cps.push_back(c);
        const uint32_t extras[] = {
            0x00AB, 0x00BB, 0x00B0, 0x00D7, 0x2013, 0x2014, 0x2022, 0x2026,
            0x2190, 0x2191, 0x2192, 0x2193, 0x25B6, 0x25BC, 0x25C0, 0x25CF, 0x2605, 0x2713, 0x2715, 0x26A0
        };
        for (uint32_t e : extras) cps.push_back(e);

        int penX = 12, penY = 8, rowH = BASE_FONT_PX + 10;
        for (uint32_t cp : cps) {
            wchar_t wc[2] = {(wchar_t)cp, 0};
            SIZE sz{};
            ::GetTextExtentPoint32W(hdc, wc, 1, &sz);
            int gw = std::max(4, (int)sz.cx + 4);
            int gh = std::max(8, (int)sz.cy + 2);
            if (penX + gw + 4 >= ATLAS_W) {
                penX = 8;
                penY += rowH;
            }
            if (penY + gh >= ATLAS_H) break;

            ::TextOutW(hdc, penX + 1, penY, wc, 1);

            GlyphInfo gi{};
            gi.u0 = float(penX) / ATLAS_W;
            gi.v0 = float(penY) / ATLAS_H;
            gi.u1 = float(penX + gw) / ATLAS_W;
            gi.v1 = float(penY + gh) / ATLAS_H;
            gi.w = (float)gw;
            gi.h = (float)gh;
            gi.advance = (float)sz.cx;
            glyphs[cp] = gi;
            penX += gw + 4;
        }

        // Конвертируем яркость глифа в чистый RGBA8 + белый блок 6x6 в левом верхнем углу
        std::vector<uint32_t> rgba(ATLAS_W * ATLAS_H, 0);
        for (int i = 0; i < ATLAS_W * ATLAS_H; ++i) {
            uint32_t p = dibPixels[i];
            uint32_t lum = std::max({(p >> 16) & 0xFFu, (p >> 8) & 0xFFu, p & 0xFFu});
            rgba[i] = (lum << 24) | 0x00FFFFFFu;
        }
        for (int y = 0; y < 6; ++y)
            for (int x = 0; x < 6; ++x)
                rgba[y * ATLAS_W + x] = 0xFFFFFFFFu;

        ::SelectObject(hdc, oldFont);
        ::DeleteObject(hFont);
        ::SelectObject(hdc, oldBm);
        ::DeleteObject(hbm);
        ::DeleteDC(hdc);

        D3D11_TEXTURE2D_DESC td{};
        td.Width = ATLAS_W; td.Height = ATLAS_H;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc = {1, 0};
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd{rgba.data(), ATLAS_W * 4, 0};
        ID3D11Texture2D* tex = nullptr;
        if (FAILED(dev->CreateTexture2D(&td, &sd, &tex))) return false;
        dev->CreateShaderResourceView(tex, nullptr, &atlasSRV);
        tex->Release();
        return atlasSRV != nullptr;
    }

    void begin() { batch.clear(); }

    void pushQuadUV(float x0, float y0, float x1, float y1,
                    float u0, float v0, float u1, float v1,
                    const Color4& cTop, const Color4& cBot) {
        if (batch.size() + 6 > MAX_UI_VERTS) return;
        batch.push_back({x0, y0, u0, v0, cTop.r, cTop.g, cTop.b, cTop.a});
        batch.push_back({x1, y0, u1, v0, cTop.r, cTop.g, cTop.b, cTop.a});
        batch.push_back({x1, y1, u1, v1, cBot.r, cBot.g, cBot.b, cBot.a});
        batch.push_back({x0, y0, u0, v0, cTop.r, cTop.g, cTop.b, cTop.a});
        batch.push_back({x1, y1, u1, v1, cBot.r, cBot.g, cBot.b, cBot.a});
        batch.push_back({x0, y1, u0, v1, cBot.r, cBot.g, cBot.b, cBot.a});
    }

    void fillRect(float x, float y, float w, float h, const Color4& c) {
        pushQuadUV(x, y, x + w, y + h, whiteU, whiteV, whiteU, whiteV, c, c);
    }

    void fillGradV(float x, float y, float w, float h, const Color4& top, const Color4& bot) {
        pushQuadUV(x, y, x + w, y + h, whiteU, whiteV, whiteU, whiteV, top, bot);
    }

    void drawRectOutline(float x, float y, float w, float h, float thick, const Color4& c) {
        fillRect(x, y, w, thick, c);
        fillRect(x, y + h - thick, w, thick, c);
        fillRect(x, y + thick, thick, h - thick * 2.f, c);
        fillRect(x + w - thick, y + thick, thick, h - thick * 2.f, c);
    }

    void drawPanel(float x, float y, float w, float h, const Color4& bgTop, const Color4& bgBot, const Color4& border) {
        fillGradV(x, y, w, h, bgTop, bgBot);
        drawRectOutline(x, y, w, h, 1.0f, border);
    }

    void drawLine(float x0, float y0, float x1, float y1, float thick, const Color4& c) {
        if (batch.size() + 6 > MAX_UI_VERTS) return;
        float dx = x1 - x0, dy = y1 - y0;
        float len = std::hypot(dx, dy);
        if (len < 0.01f) return;
        float nx = -dy / len * (thick * 0.5f);
        float ny =  dx / len * (thick * 0.5f);
        UiVert v0{x0 + nx, y0 + ny, whiteU, whiteV, c.r, c.g, c.b, c.a};
        UiVert v1{x1 + nx, y1 + ny, whiteU, whiteV, c.r, c.g, c.b, c.a};
        UiVert v2{x1 - nx, y1 - ny, whiteU, whiteV, c.r, c.g, c.b, c.a};
        UiVert v3{x0 - nx, y0 - ny, whiteU, whiteV, c.r, c.g, c.b, c.a};
        batch.push_back(v0); batch.push_back(v1); batch.push_back(v2);
        batch.push_back(v0); batch.push_back(v2); batch.push_back(v3);
    }

    void drawCircleOutline(float cx, float cy, float radius, float thick, const Color4& c, int segs = 36) {
        for (int i = 0; i < segs; ++i) {
            float a0 = (float(i) / segs) * 2.f * PI;
            float a1 = (float(i + 1) / segs) * 2.f * PI;
            drawLine(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius,
                     cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, thick, c);
        }
    }

    void fillCircle(float cx, float cy, float radius, const Color4& c, int segs = 24) {
        for (int i = 0; i < segs; ++i) {
            if (batch.size() + 3 > MAX_UI_VERTS) return;
            float a0 = (float(i) / segs) * 2.f * PI;
            float a1 = (float(i + 1) / segs) * 2.f * PI;
            batch.push_back({cx, cy, whiteU, whiteV, c.r, c.g, c.b, c.a});
            batch.push_back({cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, whiteU, whiteV, c.r, c.g, c.b, c.a});
            batch.push_back({cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, whiteU, whiteV, c.r, c.g, c.b, c.a});
        }
    }

    // Мини-флаг нации (России, Сербии, Китая, США, ЕС/НАТО, Франции, Швеции) прямо в UI!
    void drawNationFlag(int nationIdx, float x, float y, float w, float h) {
        if (nationIdx == 0) { // Россия: бело-сине-красный
            fillRect(x, y, w, h / 3.f, Color4::rgba(242, 244, 248));
            fillRect(x, y + h / 3.f, w, h / 3.f, Color4::rgba(24, 78, 178));
            fillRect(x, y + 2.f * h / 3.f, w, h / 3.f, Color4::rgba(212, 38, 38));
        } else if (nationIdx == 1) { // Сербия: красно-сине-белый + золотой герб
            fillRect(x, y, w, h / 3.f, Color4::rgba(198, 54, 60));
            fillRect(x, y + h / 3.f, w, h / 3.f, Color4::rgba(12, 64, 118));
            fillRect(x, y + 2.f * h / 3.f, w, h / 3.f, Color4::rgba(244, 245, 248));
            fillRect(x + w * 0.26f, y + h * 0.24f, w * 0.16f, h * 0.50f, Color4::rgba(232, 186, 62));
        } else if (nationIdx == 2) { // Китай
            fillRect(x, y, w, h, Color4::rgba(208, 32, 28));
            fillCircle(x + w * 0.26f, y + h * 0.36f, h * 0.20f, Color4::rgba(250, 212, 32), 10);
        } else if (nationIdx == 3) { // США
            for (int s = 0; s < 5; ++s)
                fillRect(x, y + s * (h / 5.f), w, h / 5.f, (s % 2 == 0) ? Color4::rgba(188, 34, 48) : Color4::rgba(242, 242, 246));
            fillRect(x, y, w * 0.46f, h * 0.55f, Color4::rgba(24, 52, 118));
        } else if (nationIdx == 4) { // НАТО / ЕС
            fillRect(x, y, w, h, Color4::rgba(16, 62, 138));
            drawCircleOutline(x + w * 0.5f, y + h * 0.5f, h * 0.30f, 1.5f, Color4::rgba(235, 242, 255), 16);
            drawLine(x + w * 0.2f, y + h * 0.5f, x + w * 0.8f, y + h * 0.5f, 1.2f, Color4::rgba(235, 242, 255));
        } else if (nationIdx == 5) { // Франция
            fillRect(x, y, w / 3.f, h, Color4::rgba(18, 56, 148));
            fillRect(x + w / 3.f, y, w / 3.f, h, Color4::rgba(244, 245, 248));
            fillRect(x + 2.f * w / 3.f, y, w / 3.f, h, Color4::rgba(214, 40, 46));
        } else { // Швеция
            fillRect(x, y, w, h, Color4::rgba(14, 96, 168));
            fillRect(x + w * 0.30f, y, w * 0.14f, h, Color4::rgba(250, 204, 18));
            fillRect(x, y + h * 0.40f, w, h * 0.20f, Color4::rgba(250, 204, 18));
        }
        drawRectOutline(x, y, w, h, 1.f, Color4(1, 1, 1, 0.22f));
    }

    float measureText(const char* utf8, float sizePx) const {
        if (!utf8) return 0.f;
        float scale = sizePx / BASE_FONT_PX;
        float w = 0.f;
        const char* p = utf8;
        while (uint32_t cp = nextUtf8(p)) {
            auto it = glyphs.find(cp);
            if (it == glyphs.end()) it = glyphs.find('?');
            if (it != glyphs.end()) w += it->second.advance * scale;
        }
        return w;
    }

    void drawText(const char* utf8, float x, float y, float sizePx, const Color4& col, bool shadow = true) {
        if (!utf8 || !utf8[0]) return;
        float scale = sizePx / BASE_FONT_PX;
        auto pass = [&](float ox, float oy, const Color4& c) {
            float penX = std::floor(x + ox);
            float penY = std::floor(y + oy);
            const char* p = utf8;
            while (uint32_t cp = nextUtf8(p)) {
                auto it = glyphs.find(cp);
                if (it == glyphs.end()) it = glyphs.find('?');
                if (it == glyphs.end()) continue;
                const GlyphInfo& g = it->second;
                if (cp != ' ') {
                    pushQuadUV(penX, penY, penX + g.w * scale, penY + g.h * scale,
                               g.u0, g.v0, g.u1, g.v1, c, c);
                }
                penX += g.advance * scale;
            }
        };
        if (shadow) pass(1.0f, 1.2f, Color4(0.f, 0.f, 0.f, col.a * 0.78f));
        pass(0.f, 0.f, col);
    }

    void drawTextCentered(const char* utf8, float cx, float y, float sizePx, const Color4& col, bool shadow = true) {
        float w = measureText(utf8, sizePx);
        drawText(utf8, cx - w * 0.5f, y, sizePx, col, shadow);
    }

    void drawTextRight(const char* utf8, float rx, float y, float sizePx, const Color4& col, bool shadow = true) {
        float w = measureText(utf8, sizePx);
        drawText(utf8, rx - w, y, sizePx, col, shadow);
    }

    void flush(int screenW, int screenH) {
        if (batch.empty() || !ctx) return;

        D3D11_MAPPED_SUBRESOURCE ms{};
        if (SUCCEEDED(ctx->Map(vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            std::memcpy(ms.pData, batch.data(), batch.size() * sizeof(UiVert));
            ctx->Unmap(vb, 0);
        }

        float cdata[4] = {(float)screenW, (float)screenH, 0.f, 0.f};
        ctx->UpdateSubresource(cb, 0, nullptr, cdata, 0, 0);

        D3D11_VIEWPORT vp{0.f, 0.f, (float)screenW, (float)screenH, 0.f, 1.f};
        ctx->RSSetViewports(1, &vp);
        ctx->RSSetState(rsUi);
        ctx->OMSetDepthStencilState(dssOff, 0);
        float bf[4] = {0,0,0,0};
        ctx->OMSetBlendState(bsAlpha, bf, 0xFFFFFFFF);

        ctx->IASetInputLayout(layout);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        UINT stride = sizeof(UiVert), offset = 0;
        ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(ps, nullptr, 0);
        ctx->VSSetConstantBuffers(0, 1, &cb);
        ctx->PSSetShaderResources(0, 1, &atlasSRV);
        ctx->PSSetSamplers(0, 1, &samp);
        ctx->Draw((UINT)batch.size(), 0);
        batch.clear();
    }

    void shutdown() {
        auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
        rel(rsUi); rel(bsAlpha); rel(dssOff); rel(samp);
        rel(atlasSRV); rel(cb); rel(vb); rel(layout); rel(ps); rel(vs);
    }
};

} // namespace sf
