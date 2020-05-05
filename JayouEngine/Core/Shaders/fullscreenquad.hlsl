//
// fullscreenquad.hlsl
//

cbuffer PerPassCBuffer : register(b0)
{
    float4x4 gViewProj;
    float gOverflow;
};

Texture2D gOffscreenOutput : register(t1);

SamplerState gsamPointClamp : register(s1);

static const float2 gTexCoords[6] = 
{
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
	VertexOut vout;
	
	vout.TexC = gTexCoords[vid];
	
	// Map [0,1]^2 to NDC space.
	vout.PosH = float4(2.0f*vout.TexC.x - 1.0f, 1.0f - 2.0f*vout.TexC.y, 0.0f, 1.0f);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 color = gOffscreenOutput.SampleLevel(gsamPointClamp, pin.TexC, 0.0f);
    clip(color - 0.001f);
    color = saturate(color / gOverflow);
	
    return color;
}

float4 RGBPS(VertexOut pin) : SV_Target
{
    float4 color = gOffscreenOutput.SampleLevel(gsamPointClamp, pin.TexC, 0.0f);
    // Clip small value (also reject the background pixels).
    clip(color - 0.001f);
    // Just pass vertex color into the pixel shader.
    float smooth = saturate(color.x / gOverflow);
    // trans [0, 1] to [R, G, B].
    color.r = lerp(0.0f, 1.0f, saturate(smooth * 2 - 1.0f));
    color.g = abs(abs(smooth * 2 - 1.0f) - 1.0f);
    color.b = lerp(1.0f, 0.0f, saturate(smooth * 2));
    color.a = 1.0f;
    // color = saturate(color / gOverflow);
	
	return color;
}
