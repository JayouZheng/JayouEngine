//
// FullscreenQuad.hlsl
//

#include "Common.hlsl"

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
	VertexOut vout;
	
    vout.TexC = gFullscreenQuadTexCoords[vid];
	
	// Map [0,1]^2 to NDC space.
    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 1.0f, 1.0f);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 color = gGBuffers[0].Sample(gsamAnisotropicWrap, pin.TexC).rgb;
    
    // HDR to LDR (saturate because float near 1.0f may be overflow).
    // * 1.0f is a curve control value.
    color = saturate(1.0f - exp(-color * 1.0f));
    // Gamma Correct (^2.2).
    float3 gammaCorrect = lerp(color, pow(color, 1.0f / 2.2f), 1.0f);
    
    return float4(gammaCorrect, 1.0f);
}
