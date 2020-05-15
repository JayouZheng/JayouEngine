//
// Common.hlsl
// #pragma pack_matrix( row_major ) column_major default.

#include "LightingUtil.hlsl"

#ifndef NumTextures
#define NumTextures 1024
#endif

StructuredBuffer<LightData> gLightData : register(t0, space0);
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);
// Depth, Shadow, Diffuse, Normal, ORM, Position.
Texture2D gGBuffers[6] : register(t0, space2);
Texture2D gTextures[NumTextures] : register(t0, space3);
Texture2D gCubeMaps[16] : register(t0, space4);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

static const float2 gFullscreenQuadTexCoords[6] =
{
    float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

cbuffer PerObjectCBuffer : register(b0)
{
    float4x4 gWorld;
    float4x4 gInvTWorld;
    
    int gMaterialIndex;
    int ObjectConstantPad0;
    int ObjectConstantPad1;
    int ObjectConstantPad2;
};

cbuffer PerPassCBuffer : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gShadowTransform;
    
    float3 gEyePosW;
    
    uint gNumDirLights;
    
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    
    float4 gAmbientFactor;
    float4 gFogColor;
    float4 gWireframeColor;
    
    float gFogStart;
    float gFogRange;
    
    uint gNumPointLights;
    uint gNumSpotLights;
    
    int gCubeMapIndex;
    int PassConstantPad0;
    int PassConstantPad1;
    int PassConstantPad2;
};

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}

float ConvertDepthToLinear(float4x4 Proj, float Depth)
{
    float A = Proj[2][2];
    float B = Proj[3][2];
    // Depth = A + B/z
    // => z = B / (Depth - A)
    float linearDepth = B / (Depth - A);
    return linearDepth;
}

float3 CalcWorldPosFromLinearDepth(float4x4 Proj, float4x4 InvView, float2 NDCPos, float LinearDepth)
{
    float4 position;

    position.xy = NDCPos.xy * float2(1.0f / Proj[0][0], 1.0f / Proj[1][1]) * LinearDepth;
    position.z = LinearDepth;
    position.w = 1.0;
    return mul(InvView, position).xyz;
}


//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    // In contrast, the orthographic projection transformation is completely linear
    // --- there is no divide by w. Multiplying by the orthographic projection matrix 
    // takes us directly into NDC coordinates.
    // shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gGBuffers[1].GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float) width;

    float percentLit = 0.0f;
    /*
    const float2 offsets[9] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(+dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(+dx, 0.0f),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(+dx,  +dx)
    };

    [unroll]
    for(int i = 0; i < 9; ++i)
    {
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }
    
    return percentLit / 9.0f;*/

    percentLit = gGBuffers[1].SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy, depth).r;

    return percentLit;
}

static const float2 invAtan = float2(0.1591, 0.3183);
float2 SampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5f;
    return uv;
}

float3 BoxCubeMapLookup(float3 rayOrigin, float3 unitRayDir, float3 boxCenter, float3 boxExtents)
{
    // Based on slab method as described in Real-Time Rendering   
    // 16.7.1 (3rd edition).

    // Make relative to the box center.   
    float3 p = rayOrigin - boxCenter;

    // The ith slab ray/plane intersection formulas for AABB are:
    // t1 = (-dot(n_i, p) + h_i)/dot(n_i, d) = (p_i + h_i)/d_i
    // t2 = (-dot(n_i, p) -h_i)/dot(n_i, d) = (p_i -h_i)/d_i

    // Vectorize and do ray/plane formulas for every slab together.
    float3 t1 = (-p + boxExtents) / unitRayDir;
    float3 t2 = (-p - boxExtents) / unitRayDir;

    // Find max for each coordinate. Because we assume the ray is inside
    // the box, we only want the max intersection parameter.
    float3 tmax = max(t1, t2);

    // Take minimum of all the tmax components:   
    float t = min(min(tmax.x, tmax.y), tmax.z);

    // This is relative to the box center so it can be used as a 
    // cube map lookup vector.
    return p + t * unitRayDir;
}
