//
// color.hlsl
// #pragma pack_matrix( row_major ) column_major default.

struct PerBoxData
{
    float4 Color;
};

StructuredBuffer<PerBoxData> gPerBoxData : register(t0);

cbuffer PerPassCBuffer : register(b0)
{
    float4x4 gViewProj;
    float gOverflow;
};

cbuffer PerObjectCBuffer : register(b1)
{
    float4x4 gWorld;  
    int gPerFSceneSBufferOffset;
};

struct VertexIn
{
	float4 PosL  : POSITION;
    float4 Color : COLOR;
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
    
    vout.PosW = mul(gWorld, vin.PosL);
	
	// Transform to homogeneous clip space.
    vout.PosH = mul(gViewProj, vout.PosW);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    
    return vout;
}

VertexOut BoxVS(VertexIn vin, uint vertexID : SV_VertexID)
{
    VertexOut vout;
    
    vout.PosW = mul(gWorld, vin.PosL);
	
	// Transform to homogeneous clip space.
    vout.PosH = mul(gViewProj, vout.PosW);
	
    vout.Color = gPerBoxData[gPerFSceneSBufferOffset + vertexID / 8].Color;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}
