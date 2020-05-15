//
// LightingUitl.hlsl
//

static const float PI = 3.14159265359f;

struct LightData
{
    float3 Strength;
    float FalloffStart; // point/spot light only
    
    float3 Direction; // directional/spot light only
    float FalloffEnd; // point/spot light only
    
    float3 Position; // point light only
    float SpotPower; // spot light only
};

struct MaterialData
{
    float4 DiffuseAlbedo;
		
	// Used in texture mapping.
    float4x4 MatTransform;

    float Roughness;
    float Metallicity;
    float MaterialPad0;
    float MaterialPad1;

    int DiffuseMapIndex; // if -1, Vertex Color.
    int NormalMapIndex; // if -1, Vertex Normal.
    int ORMMapIndex; // AO, Roughness, Metallicity.
    int MaterialPad2;
};

struct LightingInfo
{
    float3 Diffuse;
    float3 Normal;
    float Roughness;
    float Metallicity;
    float3 toEye;
    float3 Position;
    
    LightData Light;
};

float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

float3 Fresnel_Epic(float3 F0, float HdotV)
{
    return F0 + (1.0f - F0) * pow(2, (-5.55473f * HdotV - 6.98316f) * HdotV);
}

float NDF_GGXTR(float Roughness, float HdotN)
{
    float Rpow2 = pow(Roughness, 2);
    return Rpow2 / (PI * pow(pow(HdotN, 2) * (Rpow2 - 1) + 1, 2));
}

float G_SchlickGGX(float Roughness, float NdotL_orV)
{
    float Kg = pow(Roughness + 1, 2) / 8.0f;
    return NdotL_orV / (NdotL_orV * (1 - Kg) + Kg);
}

float3 F_Cook_Torrance(float HdotN, float NdotL, float NdotV, float Roughness)
{
    return NDF_GGXTR(Roughness, HdotN) * G_SchlickGGX(Roughness, NdotL) * G_SchlickGGX(Roughness, NdotV) / (4.0f * max(NdotL * NdotV, 0.01f));
}

float3 F_Lambert(float3 Diffuse, float Metallicity)
{
    return Diffuse * (1.0f - Metallicity) / PI;
}

float3 PBR_DirectLighting(float3 Diffuse, float3 InNormal, float Roughness, float Metallicity, float3 IntoLight, float3 IntoEye, float3 LightStrength)
{
    float3 Normal = normalize(InNormal);
    float3 toLight = normalize(IntoLight);
    float3 toEye = normalize(IntoEye);
    
    float3 F0 = 0.04f;
    F0 = lerp(F0, Diffuse, Metallicity);
    float3 halfVector = normalize(toLight + toEye);
    float HdotV = max(dot(halfVector, toEye), 0.0f);
    float HdotN = max(dot(halfVector, Normal), 0.0f);
    float NdotL = max(dot(Normal, toLight), 0.0f);
    float NdotV = max(dot(Normal, toEye), 0.0f);
    float3 Ks = Fresnel_Epic(F0, HdotV);
    float3 Kd = 1.0f - Ks;
    
    float3 Fr = Kd * F_Lambert(Diffuse, Metallicity) + Ks * F_Cook_Torrance(HdotN, NdotL, NdotV, Roughness);
    
    return Fr * LightStrength * NdotL;
}

float LinearFalloff(float Distance, float FalloffStart, float FalloffEnd)
{
    return saturate((FalloffEnd - Distance) / (FalloffEnd - FalloffStart));
}

float InverseSquareFalloff(float Distance, float FalloffStart, float FalloffEnd)
{
    float Radius = FalloffEnd - FalloffStart;
    return saturate(pow(1 - pow(Distance - Radius, 4), 2)) / (pow(Distance, 2) + 1);
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for directional lights.
//---------------------------------------------------------------------------------------
float3 ComputeDirectionalLight(LightingInfo Info)
{
    float3 toLight = normalize(-Info.Light.Direction);
    return PBR_DirectLighting(Info.Diffuse, Info.Normal, Info.Roughness, Info.Metallicity, toLight, Info.toEye, Info.Light.Strength);
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for point lights.
//---------------------------------------------------------------------------------------
float3 ComputePointLight(LightingInfo Info)
{
    // The vector from the surface to the light.
    float3 toLight = Info.Light.Position - Info.Position;

    // The distance from surface to light.
    float d = length(toLight);

    // Range test.
    if (d > Info.Light.FalloffEnd)
        return 0.0f;

    // Normalize the light vector.
    toLight /= d;

    // Attenuate light by distance.
    float falloff = InverseSquareFalloff(d, Info.Light.FalloffStart, Info.Light.FalloffEnd);

    return PBR_DirectLighting(Info.Diffuse, Info.Normal, Info.Roughness, Info.Metallicity, toLight, Info.toEye, Info.Light.Strength * falloff);
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for spot lights.
//---------------------------------------------------------------------------------------
float3 ComputeSpotLight(LightingInfo Info)
{
    // The vector from the surface to the light.
    float3 toLight = Info.Light.Position - Info.Position;

    // The distance from surface to light.
    float d = length(toLight);

    // Range test.
    if (d > Info.Light.FalloffEnd)
        return 0.0f;

    // Normalize the light vector.
    toLight /= d;

    // Attenuate light by distance.
    float falloff = InverseSquareFalloff(d, Info.Light.FalloffStart, Info.Light.FalloffEnd);
    
    float3 spotDir = normalize(Info.Light.Direction);

    // Scale by spotlight
    float spotFactor = pow(max(dot(-toLight, spotDir), 0.0f), Info.Light.SpotPower);

    return PBR_DirectLighting(Info.Diffuse, Info.Normal, Info.Roughness, Info.Metallicity, toLight, Info.toEye, Info.Light.Strength * falloff * spotFactor);
}