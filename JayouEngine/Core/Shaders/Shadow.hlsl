//
// Shadow.hlsl
//

#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	MaterialData matData = gMaterialData[gMaterialIndex];
	
    // Transform to world space.
    float4 posW = mul(gWorld, float4(vin.PosL, 1.0f));

    // Transform to homogeneous clip space.
    vout.PosH = mul(gViewProj, posW);
	
    vout.TexC = mul(matData.MatTransform, float4(vin.TexC, 0.0f, 1.0f)).xy;
	
    return vout;
}

// This is only used for alpha cut out geometry, so that shadows 
// show up correctly.  Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void PS(VertexOut pin) 
{
	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
    int diffuseMapIndex = matData.DiffuseMapIndex;
	
	// Dynamically look up the texture in the array.
    float4 diffuseMap = diffuseMapIndex >= 0 ? gTextures[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC) : 1.0f;
	diffuseAlbedo *= diffuseMap;

#ifdef ALPHA_TEST
    // Discard pixel if texture alpha < 0.1.  We do this test as soon 
    // as possible in the shader so that we can potentially exit the
    // shader early, thereby skipping the rest of the shader code.
    // clip(diffuseAlbedo.a - 0.1f);
#endif
}


