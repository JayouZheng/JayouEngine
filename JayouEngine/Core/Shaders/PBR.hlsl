//
// PBR.hlsl
//

#include "Common.hlsl"

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 NDCPos : POSITION;
    float2 TexC : TEXCOORD;
    
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
	
    float2 texCood = gFullscreenQuadTexCoords[vid];
	// Map [0, 1]^2 to NDC space [-1, 1].
    vout.PosH = float4(2.0f * texCood.x - 1.0f, 1.0f - 2.0f * texCood.y, 0.0f, 1.0f);

    vout.TexC = texCood;
    vout.NDCPos = vout.PosH.xy;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    LightingInfo info;
    
    int3 location3 = int3(pin.PosH.xy, 0);
    float depth = gGBuffers[0].Sample(gsamLinearWrap, pin.TexC).r;
    // clip(0.999f - depth);
    
    // Calc Pos From Depth.
    float linearDepth = ConvertDepthToLinear(gProj, depth);
    info.Position = CalcWorldPosFromLinearDepth(gProj, gInvView, pin.NDCPos, linearDepth);
       
    info.Diffuse = gGBuffers[2].Sample(gsamLinearWrap, pin.TexC).rgb;
    info.Normal = gGBuffers[3].Sample(gsamLinearWrap, pin.TexC).rgb;
    info.Normal = normalize(info.Normal * 2.0f - 1.0f);
    float3 ORM = gGBuffers[4].Sample(gsamLinearWrap, pin.TexC).rgb;
    // info.Position = gGBuffers[5].Sample(gsamLinearWrap, pin.TexC).rgb;
    float AO = ORM.r;
    info.Roughness = ORM.g;
    info.Metallicity = ORM.b;
    
    info.toEye = normalize(gEyePosW - info.Position);
    
    float3 color = 0.0f;
    
    // Calc Shadow Factor.
    float4 shadowPosH = mul(gShadowTransform, float4(info.Position, 1.0f));
    float shadowFactor = CalcShadowFactor(shadowPosH);
       
    for (uint i = 0; i < gNumDirLights; ++i)
    {
        info.Light = gLightData[i];
        color += shadowFactor * ComputeDirectionalLight(info);
        // The Default First Dir Cast Shadow.
        shadowFactor = 1.0f;
    }
        
    for (uint j = gNumDirLights; j < gNumDirLights + gNumPointLights; ++j)
    {
        info.Light = gLightData[j];
        color += ComputePointLight(info);
    }
    
    for (uint k = gNumDirLights + gNumPointLights; k < gNumDirLights + gNumPointLights + gNumSpotLights; ++k)
    {
        info.Light = gLightData[k];
        color += ComputeSpotLight(info);
    }
    
    if (gCubeMapIndex >= 0)
    {
         // Add in specular reflections.
        float3 r = reflect(-info.toEye, info.Normal);
        r.y = -r.y;
        float3 toLight = normalize(r);
        float2 uv = SampleSphericalMap(toLight); // make sure to normalize localPos
        float3 reflectionColor = gTextures[gCubeMapIndex].Sample(gsamLinearWrap, uv).rgb;
        
        float3 F0 = 0.04f;
        F0 = lerp(F0, info.Diffuse, info.Metallicity);
        float3 halfVector = normalize(r + info.toEye);
        float HdotV = max(dot(halfVector, info.toEye), 0.0f);
        float3 fresnelFactor = Fresnel_Epic(F0, HdotV);
        
        //reflectionColor = (1.0f - info.Roughness) * fresnelFactor * reflectionColor;
        info.Diffuse = lerp(info.Diffuse, reflectionColor, fresnelFactor);
    }
    
    color += AO * gAmbientFactor.rgb * info.Diffuse;
    
    color *= (saturate(dot(info.toEye, info.Normal))); // < 0.2f) ? 0 : 1;
        
    // HDR to LDR (saturate because float near 1.0f may be overflow).
    // * 1.0f is a curve control value.
    color = saturate(1.0f - exp(-color * 1.0f));
    // Gamma Correct (^2.2).
    float3 gammaCorrect = lerp(color, pow(color, 1.0f / 2.2f), 1.0f);
    
    return float4(gammaCorrect, 1.0f);
}