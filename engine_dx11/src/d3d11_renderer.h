#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include "math3d.h"
#include <vector>
#include <cstring>
#include <cstdio>

namespace sf {

struct Vertex3D {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b, a;
    // mx=roughness, my=metallic, mz=matTag, mw=ao
    // matTag: 0=terrain, 1=paintedArmor, 2=rubberTrack, 3=gunmetalSteel,
    //         4=concreteAsphalt, 5=foliageWood, 6=emissive, 7=eraBrick, 8=xrayModule, 9=skyDome
    float mx, my, mz, mw;
};

struct GpuMesh {
    ID3D11Buffer* vb = nullptr;
    ID3D11Buffer* ib = nullptr;
    UINT indexCount = 0;

    void release() {
        if (vb) { vb->Release(); vb = nullptr; }
        if (ib) { ib->Release(); ib = nullptr; }
        indexCount = 0;
    }
};

struct MeshBuilder {
    std::vector<Vertex3D> verts;
    std::vector<uint32_t> indices;

    void clear() { verts.clear(); indices.clear(); }

    uint32_t addVert(const Vec3& p, const Vec3& n, const Vec4& col, float rough, float metal, float tag, float ao = 1.f) {
        uint32_t idx = (uint32_t)verts.size();
        verts.push_back({p.x, p.y, p.z, n.x, n.y, n.z, col.x, col.y, col.z, col.w, rough, metal, tag, ao});
        return idx;
    }

    void addTri(uint32_t a, uint32_t b, uint32_t c) {
        indices.push_back(a); indices.push_back(b); indices.push_back(c);
    }

    void addQuad(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3,
                 const Vec4& col, float rough, float metal, float tag, float ao = 1.f) {
        Vec3 n = Vec3::cross(p1 - p0, p2 - p0).normalized();
        uint32_t i0 = addVert(p0, n, col, rough, metal, tag, ao);
        uint32_t i1 = addVert(p1, n, col, rough, metal, tag, ao);
        uint32_t i2 = addVert(p2, n, col, rough, metal, tag, ao);
        uint32_t i3 = addVert(p3, n, col, rough, metal, tag, ao);
        addTri(i0, i1, i2);
        addTri(i0, i2, i3);
    }

    // Скошенный (фасочный) 3D-блок с микро-AO на нижних гранях
    void addChamferBox(const Vec3& center, const Vec3& halfExt, float bevel,
                       const Vec4& col, float rough, float metal, float tag, float ao = 1.f,
                       const Mat4* localXf = nullptr) {
        float bx = std::min(bevel, halfExt.x * 0.45f);
        float by = std::min(bevel, halfExt.y * 0.45f);
        float bz = std::min(bevel, halfExt.z * 0.45f);
        Vec3 inH(halfExt.x - bx, halfExt.y - by, halfExt.z - bz);

        auto xfP = [&](Vec3 p) -> Vec3 {
            p = p + center;
            return localXf ? localXf->transformPoint(p) : p;
        };

        const Vec3& h0 = inH;
        const Vec3& h1 = halfExt;

        // Верх и низ
        addQuad(xfP({-h0.x,  h1.y, -h0.z}), xfP({-h0.x,  h1.y,  h0.z}), xfP({ h0.x,  h1.y,  h0.z}), xfP({ h0.x,  h1.y, -h0.z}), col, rough, metal, tag, ao);
        addQuad(xfP({-h0.x, -h1.y,  h0.z}), xfP({-h0.x, -h1.y, -h0.z}), xfP({ h0.x, -h1.y, -h0.z}), xfP({ h0.x, -h1.y,  h0.z}), col, rough, metal, tag, ao * 0.55f);
        // Перед (-Z) и корма (+Z)
        addQuad(xfP({-h0.x, -h0.y, -h1.z}), xfP({-h0.x,  h0.y, -h1.z}), xfP({ h0.x,  h0.y, -h1.z}), xfP({ h0.x, -h0.y, -h1.z}), col, rough, metal, tag, ao * 0.96f);
        addQuad(xfP({ h0.x, -h0.y,  h1.z}), xfP({ h0.x,  h0.y,  h1.z}), xfP({-h0.x,  h0.y,  h1.z}), xfP({-h0.x, -h0.y,  h1.z}), col, rough, metal, tag, ao * 0.92f);
        // Левый (-X) и правый (+X) борта
        addQuad(xfP({-h1.x, -h0.y,  h0.z}), xfP({-h1.x,  h0.y,  h0.z}), xfP({-h1.x,  h0.y, -h0.z}), xfP({-h1.x, -h0.y, -h0.z}), col, rough, metal, tag, ao * 0.95f);
        addQuad(xfP({ h1.x, -h0.y, -h0.z}), xfP({ h1.x,  h0.y, -h0.z}), xfP({ h1.x,  h0.y,  h0.z}), xfP({ h1.x, -h0.y,  h0.z}), col, rough, metal, tag, ao * 0.95f);

        if (bevel > 1e-4f) {
            float edgeRough = std::max(0.22f, rough * 0.82f); // лёгкий блик на фасках брони
            addQuad(xfP({-h0.x, h0.y, -h1.z}), xfP({-h0.x, h1.y, -h0.z}), xfP({ h0.x, h1.y, -h0.z}), xfP({ h0.x, h0.y, -h1.z}), col, edgeRough, metal, tag, ao);
            addQuad(xfP({ h0.x, h0.y,  h1.z}), xfP({ h0.x, h1.y,  h1.z}), xfP({-h0.x, h1.y,  h1.z}), xfP({-h0.x, h0.y,  h1.z}), col, edgeRough, metal, tag, ao);
            addQuad(xfP({-h1.x, h0.y,  h0.z}), xfP({-h0.x, h1.y,  h0.z}), xfP({-h0.x, h1.y, -h0.z}), xfP({-h1.x, h0.y, -h0.z}), col, edgeRough, metal, tag, ao);
            addQuad(xfP({ h1.x, h0.y, -h0.z}), xfP({ h0.x, h1.y, -h0.z}), xfP({ h0.x, h1.y,  h0.z}), xfP({ h1.x, h0.y,  h0.z}), col, edgeRough, metal, tag, ao);

            addQuad(xfP({-h1.x, -h0.y, -h0.z}), xfP({-h1.x,  h0.y, -h0.z}), xfP({-h0.x,  h0.y, -h1.z}), xfP({-h0.x, -h0.y, -h1.z}), col, edgeRough, metal, tag, ao);
            addQuad(xfP({ h0.x, -h0.y, -h1.z}), xfP({ h0.x,  h0.y, -h1.z}), xfP({ h1.x,  h0.y, -h0.z}), xfP({ h1.x, -h0.y, -h0.z}), col, edgeRough, metal, tag, ao);
            addQuad(xfP({ h1.x, -h0.y,  h0.z}), xfP({ h1.x,  h0.y,  h0.z}), xfP({ h0.x,  h0.y,  h1.z}), xfP({ h0.x, -h0.y,  h1.z}), col, edgeRough, metal, tag, ao);
            addQuad(xfP({-h0.x, -h0.y,  h1.z}), xfP({-h0.x,  h0.y,  h1.z}), xfP({-h1.x,  h0.y,  h0.z}), xfP({-h1.x, -h0.y,  h0.z}), col, edgeRough, metal, tag, ao);

            addQuad(xfP({-h0.x, -h1.y, -h0.z}), xfP({-h0.x, -h0.y, -h1.z}), xfP({ h0.x, -h0.y, -h1.z}), xfP({ h0.x, -h1.y, -h0.z}), col, rough, metal, tag, ao * 0.7f);
            addQuad(xfP({ h0.x, -h1.y,  h0.z}), xfP({ h0.x, -h0.y,  h1.z}), xfP({-h0.x, -h0.y,  h1.z}), xfP({-h0.x, -h1.y,  h0.z}), col, rough, metal, tag, ao * 0.7f);
            addQuad(xfP({-h0.x, -h1.y,  h0.z}), xfP({-h1.x, -h0.y,  h0.z}), xfP({-h1.x, -h0.y, -h0.z}), xfP({-h0.x, -h1.y, -h0.z}), col, rough, metal, tag, ao * 0.7f);
            addQuad(xfP({ h0.x, -h1.y, -h0.z}), xfP({ h1.x, -h0.y, -h0.z}), xfP({ h1.x, -h0.y,  h0.z}), xfP({ h0.x, -h1.y,  h0.z}), col, rough, metal, tag, ao * 0.7f);

            uint32_t c0 = addVert(xfP({-h0.x, h1.y, -h0.z}), Vec3(-1,1,-1).normalized(), col, edgeRough, metal, tag, ao);
            uint32_t c1 = addVert(xfP({-h1.x, h0.y, -h0.z}), Vec3(-1,1,-1).normalized(), col, edgeRough, metal, tag, ao);
            uint32_t c2 = addVert(xfP({-h0.x, h0.y, -h1.z}), Vec3(-1,1,-1).normalized(), col, edgeRough, metal, tag, ao);
            addTri(c1, c0, c2);

            c0 = addVert(xfP({ h0.x, h1.y, -h0.z}), Vec3(1,1,-1).normalized(), col, edgeRough, metal, tag, ao);
            c1 = addVert(xfP({ h0.x, h0.y, -h1.z}), Vec3(1,1,-1).normalized(), col, edgeRough, metal, tag, ao);
            c2 = addVert(xfP({ h1.x, h0.y, -h0.z}), Vec3(1,1,-1).normalized(), col, edgeRough, metal, tag, ao);
            addTri(c1, c0, c2);

            c0 = addVert(xfP({ h0.x, h1.y,  h0.z}), Vec3(1,1,1).normalized(), col, edgeRough, metal, tag, ao);
            c1 = addVert(xfP({ h1.x, h0.y,  h0.z}), Vec3(1,1,1).normalized(), col, edgeRough, metal, tag, ao);
            c2 = addVert(xfP({ h0.x, h0.y,  h1.z}), Vec3(1,1,1).normalized(), col, edgeRough, metal, tag, ao);
            addTri(c1, c0, c2);

            c0 = addVert(xfP({-h0.x, h1.y,  h0.z}), Vec3(-1,1,1).normalized(), col, edgeRough, metal, tag, ao);
            c1 = addVert(xfP({-h0.x, h0.y,  h1.z}), Vec3(-1,1,1).normalized(), col, edgeRough, metal, tag, ao);
            c2 = addVert(xfP({-h1.x, h0.y,  h0.z}), Vec3(-1,1,1).normalized(), col, edgeRough, metal, tag, ao);
            addTri(c1, c0, c2);
        }
    }

    void addPrism8(const Vec3 v[8], const Vec4& col, float rough, float metal, float tag, float ao = 1.f) {
        addQuad(v[4], v[7], v[6], v[5], col, rough, metal, tag, ao);
        addQuad(v[0], v[1], v[2], v[3], col, rough, metal, tag, ao * 0.55f);
        addQuad(v[0], v[4], v[5], v[1], col, rough, metal, tag, ao * 0.98f);
        addQuad(v[2], v[6], v[7], v[3], col, rough, metal, tag, ao * 0.92f);
        addQuad(v[3], v[7], v[4], v[0], col, rough, metal, tag, ao * 0.95f);
        addQuad(v[1], v[5], v[6], v[2], col, rough, metal, tag, ao * 0.95f);
    }

    void addCylinder(const Vec3& center, float r0, float r1, float length, int segs, int axis,
                     const Vec4& col, float rough, float metal, float tag, float ao = 1.f,
                     bool caps = true, const Mat4* localXf = nullptr) {
        float halfL = length * 0.5f;
        auto mapAxis = [&](float u, float v, float w) -> Vec3 {
            Vec3 p;
            if (axis == 0) p = Vec3(center.x + w, center.y + u, center.z + v);
            else if (axis == 1) p = Vec3(center.x + u, center.y + w, center.z + v);
            else p = Vec3(center.x + u, center.y + v, center.z + w);
            return localXf ? localXf->transformPoint(p) : p;
        };
        auto mapNorm = [&](float u, float v, float w) -> Vec3 {
            Vec3 n;
            if (axis == 0) n = Vec3(w, u, v);
            else if (axis == 1) n = Vec3(u, w, v);
            else n = Vec3(u, v, w);
            return (localXf ? localXf->transformDir(n) : n).normalized();
        };

        float slope = (r0 - r1) / std::max(0.001f, length);
        uint32_t base = (uint32_t)verts.size();
        for (int i = 0; i <= segs; ++i) {
            float a = (float(i) / segs) * 2.f * PI;
            float ca = std::cos(a), sa = std::sin(a);
            Vec3 n = mapNorm(ca, sa, slope);
            addVert(mapAxis(ca * r0, sa * r0, -halfL), n, col, rough, metal, tag, ao);
            addVert(mapAxis(ca * r1, sa * r1,  halfL), n, col, rough, metal, tag, ao);
        }
        for (int i = 0; i < segs; ++i) {
            uint32_t i0 = base + i * 2;
            addTri(i0, i0 + 2, i0 + 1);
            addTri(i0 + 1, i0 + 2, i0 + 3);
        }
        if (caps) {
            if (r0 > 1e-4f) {
                Vec3 n0 = mapNorm(0, 0, -1);
                uint32_t c0 = addVert(mapAxis(0, 0, -halfL), n0, col, rough, metal, tag, ao);
                for (int i = 0; i < segs; ++i) {
                    float a0 = (float(i) / segs) * 2.f * PI;
                    float a1 = (float(i + 1) / segs) * 2.f * PI;
                    uint32_t v0 = addVert(mapAxis(std::cos(a0)*r0, std::sin(a0)*r0, -halfL), n0, col, rough, metal, tag, ao);
                    uint32_t v1 = addVert(mapAxis(std::cos(a1)*r0, std::sin(a1)*r0, -halfL), n0, col, rough, metal, tag, ao);
                    addTri(c0, v1, v0);
                }
            }
            if (r1 > 1e-4f) {
                Vec3 n1 = mapNorm(0, 0, 1);
                uint32_t c1 = addVert(mapAxis(0, 0, halfL), n1, col, rough, metal, tag, ao);
                for (int i = 0; i < segs; ++i) {
                    float a0 = (float(i) / segs) * 2.f * PI;
                    float a1 = (float(i + 1) / segs) * 2.f * PI;
                    uint32_t v0 = addVert(mapAxis(std::cos(a0)*r1, std::sin(a0)*r1, halfL), n1, col, rough, metal, tag, ao);
                    uint32_t v1 = addVert(mapAxis(std::cos(a1)*r1, std::sin(a1)*r1, halfL), n1, col, rough, metal, tag, ao);
                    addTri(c1, v0, v1);
                }
            }
        }
    }

    void addEllipsoid(const Vec3& center, const Vec3& radii, int latSegs, int lonSegs,
                      bool upperHalfOnly, const Vec4& col, float rough, float metal, float tag, float ao = 1.f) {
        int latEnd = upperHalfOnly ? (latSegs / 2) : latSegs;
        uint32_t base = (uint32_t)verts.size();
        for (int y = 0; y <= latEnd; ++y) {
            float v = float(y) / latSegs;
            float phi = v * PI;
            float sp = std::sin(phi), cp = std::cos(phi);
            for (int x = 0; x <= lonSegs; ++x) {
                float u = float(x) / lonSegs;
                float th = u * 2.f * PI;
                Vec3 unit(sp * std::cos(th), cp, sp * std::sin(th));
                Vec3 p(center.x + unit.x * radii.x, center.y + unit.y * radii.y, center.z + unit.z * radii.z);
                Vec3 n(unit.x / std::max(0.01f, radii.x), unit.y / std::max(0.01f, radii.y), unit.z / std::max(0.01f, radii.z));
                addVert(p, n.normalized(), col, rough, metal, tag, ao);
            }
        }
        int stride = lonSegs + 1;
        for (int y = 0; y < latEnd; ++y) {
            for (int x = 0; x < lonSegs; ++x) {
                uint32_t i0 = base + y * stride + x;
                uint32_t i1 = i0 + 1;
                uint32_t i2 = i0 + stride;
                uint32_t i3 = i2 + 1;
                addTri(i0, i1, i2);
                addTri(i1, i3, i2);
            }
        }
    }

    GpuMesh upload(ID3D11Device* dev) const {
        GpuMesh gm;
        if (verts.empty() || indices.empty() || !dev) return gm;

        D3D11_BUFFER_DESC vbd{};
        vbd.ByteWidth = (UINT)(verts.size() * sizeof(Vertex3D));
        vbd.Usage = D3D11_USAGE_IMMUTABLE;
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vsd{verts.data(), 0, 0};
        dev->CreateBuffer(&vbd, &vsd, &gm.vb);

        D3D11_BUFFER_DESC ibd{};
        ibd.ByteWidth = (UINT)(indices.size() * sizeof(uint32_t));
        ibd.Usage = D3D11_USAGE_IMMUTABLE;
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA isd{indices.data(), 0, 0};
        dev->CreateBuffer(&ibd, &isd, &gm.ib);

        gm.indexCount = (UINT)indices.size();
        return gm;
    }
};

struct alignas(16) CBFrame {
    Mat4  viewProj;
    Mat4  lightViewProj;
    Vec4  camPosTime;      // xyz=camPos, w=timeSec
    Vec4  sunDirQuality;   // xyz=sunDir(to light), w=shadowEnabled
    Vec4  sunColorExposure;// xyz=sunColor, w=exposure
    Vec4  skyTopFogStart;  // xyz=skyTop(sRGB), w=fogStart
    Vec4  skyHorizFogEnd;  // xyz=skyHorizon(sRGB), w=fogEnd
    Vec4  groundAmbMode;   // xyz=groundBounce(sRGB), w=globalRenderMode (0=PBR, 1=ThermalFLIR)
};

struct alignas(16) CBObject {
    Mat4  world;
    Vec4  tintAlpha;       // xyz=tint/camoColA(sRGB), w=alphaMult
    Vec4  camoColBPattern; // xyz=camoColB(sRGB), w=camoPattern(0..5)
    Vec4  camoColCObjMode; // xyz=camoColC(sRGB), w=objMode(0=normal, 1=hotTarget, 2=armorInsp, 3=xrayGhost)
    Vec4  armorParams;     // x=frontMM, y=sideMM, z=rearMM, w=burntFactor(0..1)
};

static const char* kDagorPbrHlsl = R"HLSL(
cbuffer CBFrame : register(b0) {
    row_major float4x4 uViewProj;
    row_major float4x4 uLightViewProj;
    float4 uCamPosTime;
    float4 uSunDirQuality;
    float4 uSunColorExp;
    float4 uSkyTopFogStart;
    float4 uSkyHorizFogEnd;
    float4 uGroundAmbMode;
};

cbuffer CBObject : register(b1) {
    row_major float4x4 uWorld;
    float4 uTintAlpha;
    float4 uCamoBPat;
    float4 uCamoCObjMode;
    float4 uArmorParams;
};

Texture2D<float> gShadowMap : register(t0);
SamplerComparisonState gShadowSamp : register(s0);

struct VSIn {
    float3 pos  : POSITION;
    float3 norm : NORMAL;
    float4 col  : COLOR0;
    float4 mat  : TEXCOORD0;
};

struct PSIn {
    float4 svPos     : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float3 localPos  : TEXCOORD1;
    float3 worldNorm : TEXCOORD2;
    float4 col       : COLOR0;
    float4 mat       : TEXCOORD3;
    float4 shadowPos : TEXCOORD4;
};

float3 srgbToLinear(float3 c) {
    return pow(max(c, 0.0001), 2.2);
}

float hash31(float3 p) {
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float noise3D(float3 p) {
    float3 i = floor(p);
    float3 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash31(i + float3(0,0,0));
    float n100 = hash31(i + float3(1,0,0));
    float n010 = hash31(i + float3(0,1,0));
    float n110 = hash31(i + float3(1,1,0));
    float n001 = hash31(i + float3(0,0,1));
    float n101 = hash31(i + float3(1,0,1));
    float n011 = hash31(i + float3(0,1,1));
    float n111 = hash31(i + float3(1,1,1));
    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);
    return lerp(lerp(nx00, nx10, f.y), lerp(nx01, nx11, f.y), f.z);
}

float fbm3(float3 p) {
    return noise3D(p) * 0.54 + noise3D(p * 2.13) * 0.28 + noise3D(p * 4.51) * 0.18;
}

PSIn VSMain(VSIn vin) {
    PSIn o;
    float4 wp = mul(float4(vin.pos, 1.0), uWorld);
    o.worldPos = wp.xyz;
    o.localPos = vin.pos;
    o.worldNorm = normalize(mul(float4(vin.norm, 0.0), uWorld).xyz);
    o.svPos = mul(wp, uViewProj);
    o.col = vin.col;
    o.mat = vin.mat;
    o.shadowPos = mul(wp, uLightViewProj);
    return o;
}

float4 VSShadow(VSIn vin) : SV_Position {
    float4 wp = mul(float4(vin.pos, 1.0), uWorld);
    return mul(wp, uLightViewProj);
}

// Атмосферное небо Dagor daSkies2: рэлеевское + Ми рассеяние, объёмные облака clouds2 и звёзды daStars
float3 evalSkyAtmosphere(float3 dir, float3 L, float timeSec) {
    float3 zenCol = srgbToLinear(uSkyTopFogStart.xyz);
    float3 horCol = srgbToLinear(uSkyHorizFogEnd.xyz);
    float h = saturate(dir.y);
    float3 sky = lerp(horCol, zenCol, pow(max(0.0001, h), 0.52));

    // Анизотропная фазовая функция Хеньи-Гринштейна (рассеяние Ми вокруг солнца/луны)
    float sunDot = dot(dir, L);
    float g = 0.76;
    float hg = (1.0 - g * g) / (4.0 * 3.14159265 * pow(max(0.02, 1.0 + g * g - 2.0 * g * sunDot), 1.5));
    float sunsetWarmth = saturate(1.0 - L.y * 1.65);
    float3 mieTint = lerp(float3(1.0, 0.88, 0.72), float3(1.0, 0.36, 0.12), sunsetWarmth);
    sky += mieTint * saturate(hg * 0.22) * (0.45 + 0.85 * sunsetWarmth);

    // Огненная заря у горизонта на закате (daSkies Golden Hour)
    float horizGlow = pow(max(0.0001, 1.0 - h), 4.2) * pow(max(0.0001, saturate(sunDot * 0.5 + 0.5)), 2.4);
    sky += float3(1.15, 0.38, 0.10) * horizGlow * sunsetWarmth * 0.85;

    // Солнечный / лунный диск
    sky += mieTint * pow(max(0.0001, saturate(sunDot)), 18.0) * 0.55;
    sky += float3(1.6, 1.35, 1.02) * smoothstep(0.9991, 0.9998, sunDot) * 5.2;

    // Звёздный каталог daStars в ночном режиме
    if (L.y < 0.12 && dir.y > 0.06) {
        float starCell = hash31(floor(dir * 290.0));
        if (starCell > 0.9955) {
            float twinkle = 0.65 + 0.35 * sin(timeSec * 3.1 + starCell * 97.0);
            sky += float3(0.82, 0.90, 1.05) * (starCell - 0.9955) * 280.0 * twinkle * saturate((0.12 - L.y) * 10.0);
        }
    }

    // Двухслойные объёмные облака clouds2 с серебристой окантовкой (silver lining)
    if (dir.y > 0.02) {
        float2 cuv = dir.xz / (dir.y + 0.14);
        float3 cp = float3(cuv.x * 0.48 + timeSec * 0.011, 1.7, cuv.y * 0.48 - timeSec * 0.007);
        float c1 = fbm3(cp);
        float c2 = fbm3(cp * 2.45 + float3(4.2, 0.5, 1.8));
        float c3 = noise3D(cp * 5.6);
        float baseMacro = c1 * 0.64 + c2 * 0.26 + c3 * 0.10;
        float dens = smoothstep(0.46, 0.74, baseMacro) * smoothstep(0.02, 0.18, dir.y);
        float litCloud = smoothstep(0.38, 0.74, fbm3(cp + L * 0.26));
        float3 darkBase = zenCol * 0.58 + horCol * 0.22;
        float3 brightTop = lerp(float3(0.96, 0.94, 0.91), float3(1.05, 0.62, 0.34), sunsetWarmth * 0.82);
        float3 cloudCol = lerp(darkBase, brightTop, litCloud);
        // Silver-lining просвет сквозь кромку облака
        float silver = pow(max(0.0001, saturate(sunDot)), 7.0) * (1.0 - dens * 0.55);
        cloudCol += mieTint * silver * 0.85;
        sky = lerp(sky, cloudCol, dens * 0.90);
    }
    return sky;
}

float evalCloudGroundShadow(float3 worldPos, float timeSec) {
    float3 cp = float3(worldPos.x * 0.0085 + timeSec * 0.011, 1.7, worldPos.z * 0.0085 - timeSec * 0.007);
    float c = fbm3(cp);
    return lerp(1.0, 0.68, smoothstep(0.52, 0.72, c));
}

float sampleShadowPCF(float4 shadowClip, float ndotl) {
    if (uSunDirQuality.w < 0.5) return 1.0;
    float3 proj = shadowClip.xyz / max(0.0001, shadowClip.w);
    float2 uv = float2(proj.x * 0.5 + 0.5, -proj.y * 0.5 + 0.5);
    if (uv.x <= 0.01 || uv.x >= 0.99 || uv.y <= 0.01 || uv.y >= 0.99 || proj.z <= 0.0 || proj.z >= 1.0)
        return 1.0;
    float bias = max(0.0022 * (1.0 - ndotl), 0.00075);
    float sum = 0.0;
    float2 texel = float2(1.0 / 2048.0, 1.0 / 2048.0);
    [unroll] for (int y = -1; y <= 1; ++y) {
        [unroll] for (int x = -1; x <= 1; ++x) {
            sum += gShadowMap.SampleCmpLevelZero(gShadowSamp, uv + float2(x, y) * texel, proj.z - bias);
        }
    }
    return sum / 9.0;
}

float3 acesTonemap(float3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSMain(PSIn pin) : SV_Target {
    float3 L = normalize(uSunDirQuality.xyz);
    int tag = (int)round(pin.mat.z);

    // Купол неба (tag == 9)
    if (tag == 9) {
        if (uGroundAmbMode.w > 0.5) return float4(0.02, 0.05, 0.04, 1.0);
        float3 rayDir = normalize(pin.worldPos - uCamPosTime.xyz);
        float3 skyLin = evalSkyAtmosphere(rayDir, L, uCamPosTime.w) * uSunColorExp.w * 0.72;
        float3 skyOut = pow(max(acesTonemap(skyLin), 0.0001), 1.0 / 2.2);
        return float4(skyOut, 1.0);
    }

    float3 N = normalize(pin.worldNorm);
    float3 V = normalize(uCamPosTime.xyz - pin.worldPos);
    float3 H = normalize(V + L);

    float rough = saturate(pin.mat.x);
    float metal = saturate(pin.mat.y);
    float ao    = saturate(pin.mat.w);
    float objMode = uCamoCObjMode.w;

    // Самосветящиеся трассеры, вспышки, оптика и внутренние модули X-Ray
    if (tag == 6 || tag == 8) {
        float3 em = pin.col.rgb * uTintAlpha.rgb;
        if (tag == 8) {
            float rim = pow(max(0.0001, 1.0 - saturate(dot(N, V))), 1.6);
            em = em * (0.62 + 0.55 * saturate(dot(N, L))) + em * rim * 0.75;
        }
        return float4(em, saturate(pin.col.a * uTintAlpha.w));
    }

    // Рентгеновский полупрозрачный контур брони
    if (objMode > 2.5) {
        float fres = pow(max(0.0001, 1.0 - abs(dot(N, V))), 2.1);
        float3 xcol = lerp(float3(0.05, 0.17, 0.28), float3(0.32, 0.82, 1.00), fres);
        float alpha = saturate(0.11 + fres * 0.38) * uTintAlpha.w;
        return float4(xcol, alpha);
    }

    // Инспектор приведённой брони в ангаре
    if (objMode > 1.5 && objMode < 2.5) {
        float cosImp = max(0.28, abs(dot(N, V)));
        float nom = (abs(pin.localPos.x) > 1.18) ? uArmorParams.y : ((pin.localPos.z > 1.1) ? uArmorParams.z : uArmorParams.x);
        float eff = nom / cosImp;
        float t = saturate((eff - 80.0) / 780.0);
        float3 acol = (t < 0.5)
            ? lerp(float3(0.12, 0.82, 0.30), float3(0.96, 0.82, 0.14), t * 2.0)
            : lerp(float3(0.96, 0.82, 0.14), float3(0.92, 0.16, 0.12), (t - 0.5) * 2.0);
        float lit = 0.42 + 0.58 * saturate(dot(N, L));
        return float4(acol * lit, 1.0);
    }

    // Переводим базовое альбедо из sRGB в честное линейное пространство PBR!
    float3 srgbAlbedo = pin.col.rgb;

    if (tag == 1 || tag == 7) {
        srgbAlbedo *= uTintAlpha.rgb;
        int pat = (int)round(uCamoBPat.w);
        if (pat > 0) {
            float scale = (pat == 4) ? 2.4 : 0.85;
            float3 sampleP = (pat == 4) ? floor(pin.localPos * 4.5) * 0.55 : pin.localPos * scale;
            float n1 = fbm3(sampleP + float3(2.1, 0.7, 4.3));
            float n2 = fbm3(sampleP * 1.68 + float3(8.4, 3.2, 1.9));
            if (n1 > 0.52) srgbAlbedo = uCamoBPat.rgb * (tag == 7 ? 0.92 : 1.0);
            if (n2 > 0.61) srgbAlbedo = uCamoCObjMode.rgb * (tag == 7 ? 0.94 : 1.0);
        }
    }

    float3 albedo = srgbToLinear(srgbAlbedo);
    float  geomEdge = saturate(length(fwidth(normalize(pin.worldNorm))) * 2.65);

    if (tag == 1 || tag == 7) {
        // Настоящая матовая военная эмаль CARC (диэлектрик metal=0.04, rough=0.73 — НИКАКОГО пластилинового полуметалла!)
        metal = 0.04;
        rough = (tag == 7) ? 0.69 : 0.74;

        // Микрорельеф катаной/литой броневой стали + антискользящее покрытие на крышах корпуса и башни
        float castBumpX = (noise3D(pin.localPos * 46.0 + float3(1.7, 0.0, 0.0)) - 0.5);
        float castBumpZ = (noise3D(pin.localPos * 46.0 + float3(0.0, 0.0, 3.1)) - 0.5);
        float antiSlip  = (pin.worldNorm.y > 0.78) ? (noise3D(pin.localPos * 110.0) - 0.5) * 0.16 : 0.0;
        N = normalize(N + float3(castBumpX * 0.11 + antiSlip, antiSlip * 0.4, castBumpZ * 0.11 - antiSlip));
        albedo *= (0.95 + castBumpX * 0.14);

        // Тёмные технологические швы между бронеплитами, люками и блоками ДЗ + головки болтов
        if (tag == 7) {
            float seamZ = smoothstep(0.88, 0.98, abs(sin(pin.localPos.z * 10.5)));
            float seamX = smoothstep(0.88, 0.98, abs(sin(pin.localPos.x * 10.5)));
            float seam  = max(seamZ, seamX);
            ao *= (1.0 - seam * 0.58);
            rough = lerp(rough, 0.92, seam * 0.6);
        } else {
            float plateSeam = smoothstep(0.955, 0.995, abs(sin(pin.localPos.z * 2.8)));
            ao *= (1.0 - plateSeam * 0.42);
        }

        // Сколы краски до ХОЛОДНОЙ КАТАНОЙ СТАЛИ строго по реальным 3D-рёбрам геометрии (fwidth нормали)!
        float chipNoise = smoothstep(0.32, 0.76, noise3D(pin.localPos * 24.0));
        float steelChip = saturate(geomEdge * (0.45 + 0.85 * chipNoise));
        albedo = lerp(albedo, float3(0.36, 0.38, 0.42), steelChip * 0.78);
        metal  = lerp(metal,  0.92, steelChip * 0.88);
        rough  = lerp(rough,  0.24, steelChip * 0.82);

        // Сухая дорожная пыль и комья грязи снизу на бортах и НЛД
        float mudMask = saturate((0.82 - pin.localPos.y) * 1.55) * smoothstep(0.35, 0.78, fbm3(pin.localPos * 4.2));
        albedo = lerp(albedo, srgbToLinear(float3(0.24, 0.20, 0.15)), mudMask * 0.58);
        rough  = lerp(rough, 0.94, mudMask * 0.75);
        metal  = lerp(metal, 0.02, mudMask * 0.85);

        // Глубокое контактное затенение днища, погона башни и подкрылков
        ao *= lerp(0.32, 1.0, saturate(pin.localPos.y * 1.05));

        if (uArmorParams.w > 0.01) {
            albedo = lerp(albedo, float3(0.008, 0.007, 0.007), saturate(uArmorParams.w));
            rough = 0.95;
            metal = 0.05;
        }
    }
    else if (tag == 3) {
        // Оружейная воронёная и точёная сталь (стволы, казённики, ЗПУ, звёздочки, буксирные крюки)
        float lathe = sin(pin.localPos.z * 85.0) * 0.04;
        albedo = max(albedo, float3(0.11, 0.12, 0.135)) * (1.0 + lathe);
        albedo = lerp(albedo, float3(0.44, 0.46, 0.50), geomEdge * 0.65);
        metal  = 0.88;
        rough  = lerp(0.31, 0.19, geomEdge);
    }
    else if (tag == 0) {
        // Многослойный ландшафт Dagor HeightMapLand: дёрн, колеи, гравий и скальные выходы
        float macro  = fbm3(pin.worldPos * 0.045);
        float detail = noise3D(pin.worldPos * 0.85);
        float micro  = noise3D(pin.worldPos * 5.6);
        N = normalize(N + float3((micro - 0.5) * 0.22, 0.0, (detail - 0.5) * 0.22));
        float slope = 1.0 - saturate(N.y);
        albedo *= (0.68 + macro * 0.58) * (0.82 + micro * 0.36);
        float3 dryGrass = srgbToLinear(float3(0.36, 0.33, 0.19));
        albedo = lerp(albedo, dryGrass, smoothstep(0.52, 0.78, macro) * 0.48);
        float3 rockCol = srgbToLinear(float3(0.32, 0.31, 0.30)) * (0.72 + detail * 0.50);
        albedo = lerp(albedo, rockCol, saturate((slope - 0.16) * 3.6));
        rough = lerp(0.93, 0.74, saturate(slope * 2.0));
        metal = 0.02;
    }
    else if (tag == 4) {
        // Аэродромные бетонные плиты ПАГ-18 с гудроновыми швами, следами траков и зеркальными лужами!
        float concreteVar = fbm3(pin.worldPos * 0.38);
        float microGrit   = noise3D(pin.worldPos * 9.5);
        float seamX = abs(frac(pin.worldPos.x * 0.1666) - 0.5);
        float seamZ = abs(frac(pin.worldPos.z * 0.1666) - 0.5);
        float seam  = (seamX > 0.485 || seamZ > 0.485) ? 0.26 : 1.0;
        N = normalize(N + float3((microGrit - 0.5) * 0.12, 0.0, (concreteVar - 0.5) * 0.12));
        albedo *= (0.76 + concreteVar * 0.44) * (0.88 + microGrit * 0.24) * seam;
        rough = 0.86;
        metal = 0.02;

        // Плотная контактная тень под гусеницами и днищем танка на плацу ангара
        if (abs(pin.worldPos.y) < 0.12) {
            float2 d0 = pin.worldPos.xz / float2(2.05, 3.75);
            float underTank = 1.0 - smoothstep(0.50, 1.22, length(d0));
            ao *= (1.0 - underTank * 0.80);
        }

        if (N.y > 0.92) {
            float puddle = smoothstep(0.56, 0.68, fbm3(pin.worldPos * 0.15 + float3(3.1, 0.0, 5.7)));
            albedo = lerp(albedo, albedo * 0.38, puddle * 0.85);
            rough  = lerp(rough, 0.045, puddle * 0.94);
            metal  = lerp(metal, 0.18, puddle * 0.50);
            N      = lerp(N, float3(0, 1, 0), puddle * 0.92);
        }
    }
    else if (tag == 2) {
        // Стальные траки с полированными грунтозацепами и матовая резина бандажей катков
        float cleat = smoothstep(0.35, 0.75, sin(pin.localPos.z * 48.0));
        bool isTrack = (metal > 0.20);
        if (isTrack) {
            albedo = lerp(float3(0.018, 0.017, 0.016), float3(0.28, 0.30, 0.33), cleat * 0.75 + geomEdge * 0.4);
            metal  = lerp(0.35, 0.88, cleat);
            rough  = lerp(0.86, 0.28, cleat);
            ao    *= lerp(0.45, 0.95, cleat);
        } else {
            // Матовая чёрная резина бандажей и экранов
            float rubGrit = noise3D(pin.localPos * 38.0);
            albedo *= (0.85 + rubGrit * 0.30);
            metal = 0.02;
            rough = 0.91;
            ao   *= 0.78;
        }
    }
    else if (tag == 5) {
        // Хвоя и кора деревьев с микрорельефом ветвей
        float leaf = noise3D(pin.worldPos * 4.5);
        float twig = noise3D(pin.worldPos * 16.0);
        N = normalize(N + float3((twig - 0.5) * 0.32, (leaf - 0.5) * 0.18, (leaf - 0.5) * 0.32));
        albedo *= (0.66 + leaf * 0.55) * (0.82 + twig * 0.35);
        rough = 0.92;
        metal = 0.01;
        ao *= (0.58 + 0.42 * saturate(N.y * 0.5 + 0.5));
    }

    // Режим ТВП (тепловизор 3-го поколения FLIR)
    if (uGroundAmbMode.w > 0.5) {
        float lum = dot(pow(max(albedo, 0.0001), 1.0 / 2.2), float3(0.299, 0.587, 0.114));
        float coldEnv = 0.05 + lum * 0.20 + (1.0 - N.y) * 0.07;
        float heat = coldEnv;
        if (objMode > 0.5 && objMode < 1.5) {
            float engineHot = saturate((pin.localPos.z - 0.4) * 0.65) * 0.35;
            float rimHot = pow(max(0.0001, 1.0 - abs(dot(N, V))), 1.5) * 0.25;
            heat = saturate(0.70 + engineHot + rimHot + (tag == 2 ? 0.22 : 0.0));
        }
        float scan = 0.95 + 0.05 * sin(pin.svPos.y * 1.57);
        float3 flir = lerp(float3(0.015, 0.045, 0.035), float3(0.94, 1.00, 0.95), saturate(heat)) * scan;
        return float4(flir, saturate(pin.col.a * uTintAlpha.w));
    }

    // Физически корректное освещение GGX + полусферический GI + отражения неба во влажном бетоне и броне
    float NdotL = saturate(dot(N, L));
    float NdotV = max(0.001, saturate(dot(N, V)));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float shadow = sampleShadowPCF(pin.shadowPos, NdotL) * evalCloudGroundShadow(pin.worldPos, uCamPosTime.w);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metal);
    float  a  = max(0.035, rough * rough);
    float  a2 = a * a;
    float  denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    float  D = a2 / max(0.0001, 3.14159265 * denom * denom);
    float  k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float  G = (NdotV / (NdotV * (1.0 - k) + k)) * (NdotL / (NdotL * (1.0 - k) + k));
    float3 F = F0 + (1.0 - F0) * pow(max(0.0001, 1.0 - VdotH), 5.0);

    float3 spec = (D * G * F) / max(0.001, 4.0 * NdotV * NdotL);
    float3 kd   = (1.0 - F) * (1.0 - metal);

    float3 sunRadiance = uSunColorExp.xyz;
    float3 direct = (kd * albedo / 3.14159265 + spec) * sunRadiance * NdotL * shadow;

    // Полусферическое рассеянное освещение неба/земли в линейном пространстве
    float3 skyZenLin = srgbToLinear(uSkyTopFogStart.xyz);
    float3 skyHorLin = srgbToLinear(uSkyHorizFogEnd.xyz);
    float3 gndAmbLin = srgbToLinear(uGroundAmbMode.xyz) * 0.55;

    float hemi = N.y * 0.5 + 0.5;
    float3 skyAmb = lerp(gndAmbLin, lerp(skyHorLin, skyZenLin, 0.62), hemi) * 0.82;

    float3 R = reflect(-V, N);
    float3 envRefl = lerp(gndAmbLin, lerp(skyHorLin, skyZenLin, saturate(R.y)), saturate(R.y * 0.5 + 0.5));
    float sunRefl = pow(max(0.0001, saturate(dot(R, L))), lerp(180.0, 12.0, rough)) * (1.0 - rough * 0.82) * shadow;
    envRefl += sunRadiance * sunRefl * 0.35;

    float3 fresEnv = F0 + (max(float3(1.0 - rough, 1.0 - rough, 1.0 - rough), F0) - F0) * pow(max(0.0001, 1.0 - NdotV), 5.0);
    float3 ambient = (kd * albedo * skyAmb + envRefl * fresEnv * (1.0 - rough * 0.68) * 0.72) * ao;

    float3 color = (direct + ambient) * uSunColorExp.w;

    // Атмосферная дымка горизонта в линейном пространстве
    float dist = length(pin.worldPos - uCamPosTime.xyz);
    float fogT = saturate((dist - uSkyTopFogStart.w) / max(1.0, uSkyHorizFogEnd.w - uSkyTopFogStart.w));
    fogT = fogT * fogT * (3.0 - 2.0 * fogT);
    float3 fogCol = lerp(skyHorLin, skyZenLin, 0.18) * 0.92;
    color = lerp(color, fogCol, fogT * 0.85);

    color = acesTonemap(color);
    color = pow(max(color, 0.0001), 1.0 / 2.2);
    return float4(color, saturate(pin.col.a * uTintAlpha.w));
}
)HLSL";

class D3D11Renderer {
public:
    HWND hwnd = nullptr;
    int width = 1280, height = 720;
    int msaaSamples = 4;
    bool vsync = true;
    bool shadowsEnabled = true;
    bool wireframeMode = false;

    ID3D11Device*           device = nullptr;
    ID3D11DeviceContext*    ctx = nullptr;
    IDXGISwapChain*         swapChain = nullptr;
    ID3D11RenderTargetView* mainRTV = nullptr;
    ID3D11Texture2D*        depthTex = nullptr;
    ID3D11DepthStencilView* mainDSV = nullptr;

    ID3D11Texture2D*          shadowTex = nullptr;
    ID3D11DepthStencilView*   shadowDSV = nullptr;
    ID3D11ShaderResourceView* shadowSRV = nullptr;
    ID3D11SamplerState*       shadowSamp = nullptr;

    ID3D11VertexShader* mainVS = nullptr;
    ID3D11VertexShader* shadowVS = nullptr;
    ID3D11PixelShader*  mainPS = nullptr;
    ID3D11InputLayout*  inputLayout = nullptr;
    ID3D11Buffer*       cbFrame = nullptr;
    ID3D11Buffer*       cbObject = nullptr;

    ID3D11RasterizerState*   rsSolid = nullptr;
    ID3D11RasterizerState*   rsWireframe = nullptr;
    ID3D11RasterizerState*   rsShadow = nullptr;
    ID3D11DepthStencilState* dssDefault = nullptr;
    ID3D11DepthStencilState* dssAlphaNoWrite = nullptr;
    ID3D11BlendState*        bsOpaque = nullptr;
    ID3D11BlendState*        bsAlpha = nullptr;

    CBFrame curFrame{};
    bool inShadowPass = false;

    bool init(HWND window, int w, int h, int reqMsaa = 4, bool reqVsync = true, bool reqShadows = true) {
        hwnd = window;
        width = std::max(64, w);
        height = std::max(64, h);
        msaaSamples = reqMsaa;
        vsync = reqVsync;
        shadowsEnabled = reqShadows;

        D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
        D3D_FEATURE_LEVEL outLevel{};
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                       levels, 2, D3D11_SDK_VERSION, &device, &outLevel, &ctx);
        if (FAILED(hr)) {
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                   levels, 2, D3D11_SDK_VERSION, &device, &outLevel, &ctx);
            if (FAILED(hr)) return false;
        }

        UINT qual = 0;
        if (msaaSamples > 1) {
            device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, msaaSamples, &qual);
            if (qual == 0) msaaSamples = 1;
        }

        IDXGIDevice* dxgiDev = nullptr;
        IDXGIAdapter* adapter = nullptr;
        IDXGIFactory* factory = nullptr;
        device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev);
        dxgiDev->GetAdapter(&adapter);
        adapter->GetParent(__uuidof(IDXGIFactory), (void**)&factory);

        DXGI_SWAP_CHAIN_DESC scd{};
        scd.BufferCount = 1;
        scd.BufferDesc.Width = width;
        scd.BufferDesc.Height = height;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferDesc.RefreshRate = {60, 1};
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = hwnd;
        scd.SampleDesc.Count = msaaSamples;
        scd.SampleDesc.Quality = (msaaSamples > 1 && qual > 0) ? (qual - 1) : 0;
        scd.Windowed = TRUE;

        hr = factory->CreateSwapChain(device, &scd, &swapChain);
        factory->Release(); adapter->Release(); dxgiDev->Release();
        if (FAILED(hr)) return false;

        if (!createRenderTargets()) return false;
        if (!createShadowResources()) return false;
        if (!compileShaders()) return false;
        createStates();
        return true;
    }

    bool createRenderTargets() {
        if (mainRTV) { mainRTV->Release(); mainRTV = nullptr; }
        if (mainDSV) { mainDSV->Release(); mainDSV = nullptr; }
        if (depthTex) { depthTex->Release(); depthTex = nullptr; }

        ID3D11Texture2D* backBuf = nullptr;
        if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuf))) return false;
        device->CreateRenderTargetView(backBuf, nullptr, &mainRTV);
        backBuf->Release();

        UINT qual = 0;
        if (msaaSamples > 1) device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, msaaSamples, &qual);

        D3D11_TEXTURE2D_DESC dtd{};
        dtd.Width = width;
        dtd.Height = height;
        dtd.MipLevels = 1;
        dtd.ArraySize = 1;
        dtd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dtd.SampleDesc.Count = msaaSamples;
        dtd.SampleDesc.Quality = (msaaSamples > 1 && qual > 0) ? (qual - 1) : 0;
        dtd.Usage = D3D11_USAGE_DEFAULT;
        dtd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (FAILED(device->CreateTexture2D(&dtd, nullptr, &depthTex))) return false;
        if (FAILED(device->CreateDepthStencilView(depthTex, nullptr, &mainDSV))) return false;
        return true;
    }

    bool createShadowResources() {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = 2048;
        td.Height = 2048;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R24G8_TYPELESS;
        td.SampleDesc = {1, 0};
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&td, nullptr, &shadowTex))) return false;

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
        dsvd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        if (FAILED(device->CreateDepthStencilView(shadowTex, &dsvd, &shadowDSV))) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
        srvd.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels = 1;
        if (FAILED(device->CreateShaderResourceView(shadowTex, &srvd, &shadowSRV))) return false;

        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        sd.BorderColor[0] = sd.BorderColor[1] = sd.BorderColor[2] = sd.BorderColor[3] = 1.f;
        sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&sd, &shadowSamp);
        return true;
    }

    bool compileShaders() {
        ID3DBlob *vsBlob = nullptr, *shBlob = nullptr, *psBlob = nullptr, *errBlob = nullptr;
        UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;

        if (FAILED(D3DCompile(kDagorPbrHlsl, std::strlen(kDagorPbrHlsl), "dagor_pbr.hlsl", nullptr, nullptr,
                              "VSMain", "vs_5_0", flags, 0, &vsBlob, &errBlob))) {
            if (errBlob) { ::MessageBoxA(hwnd, (const char*)errBlob->GetBufferPointer(), "HLSL VS Error", MB_OK); errBlob->Release(); }
            return false;
        }
        if (FAILED(D3DCompile(kDagorPbrHlsl, std::strlen(kDagorPbrHlsl), "dagor_pbr.hlsl", nullptr, nullptr,
                              "VSShadow", "vs_5_0", flags, 0, &shBlob, &errBlob))) {
            if (errBlob) errBlob->Release();
            return false;
        }
        if (FAILED(D3DCompile(kDagorPbrHlsl, std::strlen(kDagorPbrHlsl), "dagor_pbr.hlsl", nullptr, nullptr,
                              "PSMain", "ps_5_0", flags, 0, &psBlob, &errBlob))) {
            if (errBlob) { ::MessageBoxA(hwnd, (const char*)errBlob->GetBufferPointer(), "HLSL PS Error", MB_OK); errBlob->Release(); }
            return false;
        }

        device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &mainVS);
        device->CreateVertexShader(shBlob->GetBufferPointer(), shBlob->GetBufferSize(), nullptr, &shadowVS);
        device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &mainPS);

        D3D11_INPUT_ELEMENT_DESC ied[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        device->CreateInputLayout(ied, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
        vsBlob->Release(); shBlob->Release(); psBlob->Release();

        D3D11_BUFFER_DESC cbd{};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.ByteWidth = sizeof(CBFrame);
        device->CreateBuffer(&cbd, nullptr, &cbFrame);
        cbd.ByteWidth = sizeof(CBObject);
        device->CreateBuffer(&cbd, nullptr, &cbObject);
        return true;
    }

    void createStates() {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.MultisampleEnable = (msaaSamples > 1);
        rd.DepthClipEnable = TRUE;
        device->CreateRasterizerState(&rd, &rsSolid);

        D3D11_RASTERIZER_DESC wrd = rd;
        wrd.FillMode = D3D11_FILL_WIREFRAME;
        device->CreateRasterizerState(&wrd, &rsWireframe);

        rd.DepthBias = 900;
        rd.SlopeScaledDepthBias = 1.8f;
        device->CreateRasterizerState(&rd, &rsShadow);

        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        device->CreateDepthStencilState(&dsd, &dssDefault);

        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        device->CreateDepthStencilState(&dsd, &dssAlphaNoWrite);

        D3D11_BLEND_DESC bd{};
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&bd, &bsOpaque);

        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        device->CreateBlendState(&bd, &bsAlpha);
    }

    void onResize(int w, int h) {
        if (!swapChain || w <= 0 || h <= 0) return;
        width = w; height = h;
        ctx->OMSetRenderTargets(0, nullptr, nullptr);
        if (mainRTV) { mainRTV->Release(); mainRTV = nullptr; }
        if (mainDSV) { mainDSV->Release(); mainDSV = nullptr; }
        if (depthTex) { depthTex->Release(); depthTex = nullptr; }
        swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        createRenderTargets();
    }

    void beginShadowPass(const CBFrame& frameData) {
        curFrame = frameData;
        inShadowPass = true;
        ID3D11ShaderResourceView* nullSRV = nullptr;
        ctx->PSSetShaderResources(0, 1, &nullSRV);

        ctx->OMSetRenderTargets(0, nullptr, shadowDSV);
        ctx->ClearDepthStencilView(shadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
        D3D11_VIEWPORT vp{0.f, 0.f, 2048.f, 2048.f, 0.f, 1.f};
        ctx->RSSetViewports(1, &vp);
        ctx->RSSetState(rsShadow);
        ctx->OMSetDepthStencilState(dssDefault, 0);
        ctx->IASetInputLayout(inputLayout);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(shadowVS, nullptr, 0);
        ctx->PSSetShader(nullptr, nullptr, 0);
        ctx->UpdateSubresource(cbFrame, 0, nullptr, &curFrame, 0, 0);
        ctx->VSSetConstantBuffers(0, 1, &cbFrame);
        ctx->VSSetConstantBuffers(1, 1, &cbObject);
    }

    void beginMainPass(const CBFrame& frameData, const Vec4& clearSkyCol) {
        curFrame = frameData;
        inShadowPass = false;
        ctx->OMSetRenderTargets(1, &mainRTV, mainDSV);
        float clr[4] = {clearSkyCol.x, clearSkyCol.y, clearSkyCol.z, 1.f};
        ctx->ClearRenderTargetView(mainRTV, clr);
        ctx->ClearDepthStencilView(mainDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        D3D11_VIEWPORT vp{0.f, 0.f, (float)width, (float)height, 0.f, 1.f};
        ctx->RSSetViewports(1, &vp);
        ctx->RSSetState((wireframeMode && rsWireframe) ? rsWireframe : rsSolid);
        ctx->OMSetDepthStencilState(dssDefault, 0);
        float bf[4] = {0,0,0,0};
        ctx->OMSetBlendState(bsOpaque, bf, 0xFFFFFFFF);

        ctx->IASetInputLayout(inputLayout);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(mainVS, nullptr, 0);
        ctx->PSSetShader(mainPS, nullptr, 0);
        ctx->UpdateSubresource(cbFrame, 0, nullptr, &curFrame, 0, 0);
        ctx->VSSetConstantBuffers(0, 1, &cbFrame);
        ctx->PSSetConstantBuffers(0, 1, &cbFrame);
        ctx->VSSetConstantBuffers(1, 1, &cbObject);
        ctx->PSSetConstantBuffers(1, 1, &cbObject);
        ctx->PSSetShaderResources(0, 1, &shadowSRV);
        ctx->PSSetSamplers(0, 1, &shadowSamp);
    }

    void beginSubViewport3D(float x, float y, float w, float h, const CBFrame& subFrame) {
        curFrame = subFrame;
        inShadowPass = false;
        ctx->ClearDepthStencilView(mainDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
        D3D11_VIEWPORT vp{x, y, w, h, 0.f, 1.f};
        ctx->RSSetViewports(1, &vp);
        ctx->RSSetState(rsSolid);
        ctx->IASetInputLayout(inputLayout);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(mainVS, nullptr, 0);
        ctx->PSSetShader(mainPS, nullptr, 0);
        ctx->UpdateSubresource(cbFrame, 0, nullptr, &curFrame, 0, 0);
        ctx->VSSetConstantBuffers(0, 1, &cbFrame);
        ctx->PSSetConstantBuffers(0, 1, &cbFrame);
        ctx->VSSetConstantBuffers(1, 1, &cbObject);
        ctx->PSSetConstantBuffers(1, 1, &cbObject);
    }

    void setAlphaBlend(bool enable) {
        float bf[4] = {0,0,0,0};
        ctx->OMSetBlendState(enable ? bsAlpha : bsOpaque, bf, 0xFFFFFFFF);
        ctx->OMSetDepthStencilState(enable ? dssAlphaNoWrite : dssDefault, 0);
    }

    void drawMesh(const GpuMesh& mesh, const CBObject& obj) {
        if (!mesh.vb || !mesh.ib || mesh.indexCount == 0) return;
        ctx->UpdateSubresource(cbObject, 0, nullptr, &obj, 0, 0);
        UINT stride = sizeof(Vertex3D), offset = 0;
        ctx->IASetVertexBuffers(0, 1, &mesh.vb, &stride, &offset);
        ctx->IASetIndexBuffer(mesh.ib, DXGI_FORMAT_R32_UINT, 0);
        ctx->DrawIndexed(mesh.indexCount, 0, 0);
    }

    void present() {
        if (swapChain) swapChain->Present(vsync ? 1 : 0, 0);
    }

    void shutdown() {
        auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
        rel(bsAlpha); rel(bsOpaque); rel(dssAlphaNoWrite); rel(dssDefault);
        rel(rsShadow); rel(rsWireframe); rel(rsSolid); rel(cbObject); rel(cbFrame);
        rel(inputLayout); rel(mainPS); rel(shadowVS); rel(mainVS);
        rel(shadowSamp); rel(shadowSRV); rel(shadowDSV); rel(shadowTex);
        rel(mainDSV); rel(depthTex); rel(mainRTV); rel(swapChain);
        rel(ctx); rel(device);
    }
};

} // namespace sf
