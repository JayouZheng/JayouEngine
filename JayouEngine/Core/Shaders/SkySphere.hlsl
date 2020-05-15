//
// SkySphere.hlsl
//

#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};
 
VertexOut VS(VertexIn vin)
{
    VertexOut vout;

	// Use local vertex position as cubemap lookup vector.
    vout.PosL = vin.PosL;
    vout.PosL.y = -vin.PosL.y;
	
	// Transform to world space.
    float4 posW = mul(gWorld, float4(vin.PosL, 1.0f));

	// Always center sky about camera.
    posW.xyz += gEyePosW;

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
    vout.PosH = mul(gViewProj, posW).xyww;
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float2 uv = SampleSphericalMap(normalize(pin.PosL)); // make sure to normalize localPos
    float3 color = gTextures[gCubeMapIndex].Sample(gsamAnisotropicWrap, uv).rgb;
    return float4(color, 1.0f);
}

