//
// GBuffer.hlsl
//

#include "Common.hlsl"

struct VertexIn
{
    float4 Color : COLOR;
    float3 PosL : POSITION;
    float3 NormalL : NORMAL; 
    float3 TangentU : TANGENT;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{  
    float4 Color : COLOR;
    float4 PosH : SV_POSITION;    
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;  
};

struct GBufferOut
{
    float4 DiffuseTarget : SV_Target0;
    float3 NormalTarget : SV_Target1;
    float3 ORMTarget : SV_Target2;
    float4 Pos : SV_Target3;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

	// Fetch the material data.
    MaterialData matData = gMaterialData[gMaterialIndex];
    
    // Transform to world space.
    float4 posW = mul(gWorld, float4(vin.PosL, 1.0f));
    vout.PosW = posW.xyz;

    // If nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul((float3x3) gInvTWorld, vin.NormalL);
    vout.TangentW = mul((float3x3) gInvTWorld, vin.TangentU);

    // Transform to homogeneous clip space.
    vout.PosH = mul(gViewProj, posW);
	
    vout.TexC = mul(matData.MatTransform, float4(vin.TexC, 0.0f, 1.0f)).xy;
    
    vout.Color = vin.Color;
	
    return vout;
}

GBufferOut PS(VertexOut pin)
{
    GBufferOut gBuffer;
    
    // Fetch the material data.
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuse = matData.DiffuseAlbedo;
    float roughness = matData.Roughness;
    float metallicity = matData.Metallicity;
    uint diffuseMapIndex = matData.DiffuseMapIndex;
    uint normalMapIndex = matData.NormalMapIndex;
    uint ORMMapIndex = matData.ORMMapIndex;
	
	// Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);
	
    float4 diffuseMap = diffuseMapIndex != -1 ? gTextures[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC) : 1.0f;
    float3 normalMapW = pin.NormalW;
    if (normalMapIndex != -1)
    {
        float3 normalMap = gTextures[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).rgb;
        normalMapW = NormalSampleToWorldSpace(normalMap, pin.NormalW, pin.TangentW);
    }        
    float3 ORMMapMap = ORMMapIndex != -1 ? gTextures[ORMMapIndex].Sample(gsamAnisotropicWrap, pin.TexC).rgb : 1.0f;

    gBuffer.DiffuseTarget = diffuse * diffuseMap;
    gBuffer.NormalTarget = normalMapW * 0.5f + 0.5f; // [-1, 1] to [0, 1].
    gBuffer.ORMTarget = float3(ORMMapMap.r, ORMMapMap.g * roughness, ORMMapMap.b * metallicity);

    gBuffer.Pos = float4(pin.PosW, 1.0f);
    
    return gBuffer;
}
