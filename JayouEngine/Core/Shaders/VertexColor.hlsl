//
// VertexColor.hlsl
//

#include "Common.hlsl"

struct VertexIn
{
    float4 Color : COLOR;
	float3 PosL  : POSITION;  
};

struct VertexOut
{
	float4 PosH  : SV_POSITION;
    float4 PosW  : POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin, uint vertexID : SV_VertexID)
{
	VertexOut vout;
    
    vout.PosW = mul(float4(vin.PosL, 1.0f), gWorld);
	
	// Transform to homogeneous clip space.
    vout.PosH = mul(gViewProj, vout.PosW);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    
    return vout;
}

VertexOut WireframeVS(VertexIn vin, uint vertexID : SV_VertexID)
{
    VertexOut vout;
    
    vout.PosW = mul(gWorld, float4(vin.PosL, 1.0f));
	
	// Transform to homogeneous clip space.
    vout.PosH = mul(gViewProj, vout.PosW);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = gWireframeColor;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}