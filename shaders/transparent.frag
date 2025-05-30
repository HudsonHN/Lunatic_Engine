#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "struct_definitions.glsl"

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec3 inTangent;
layout (location = 5) in flat float inTangentW;
layout (location = 6) in flat uint materialIndex;
layout (location = 7) in vec3 inViewPos;
layout (location = 8) in vec4 inLightSpacePos[NUM_CASCADES];

layout (location = 0) out vec4 outFragColor;

layout(std430, set = 0, binding = 1) readonly buffer ImageMetaInfoBuffer
{   
	ImageMetaInfo imageMetaInfo[];
};

layout(std430, set = 0, binding = 2) readonly buffer Lights
{
	Light lights[];
};

layout(set = 0, binding = 3) uniform samplerCube cubeMap;

layout(set = 0, binding = 4) uniform sampler2D textures[];

layout(set = 1, binding = 0) uniform CascadeUBO
{
	mat4 lightSpaceTransform[NUM_CASCADES];
};

layout(set = 1, binding = 1) uniform sampler2D dirShadowMaps[];

layout(set = 2, binding = 0) uniform SceneDataBuffer{   
	SceneData sceneData;
};

const float PI = 3.14159265359f;

/** BLINN PHONG (UNUSED) **/
vec3 BlinnPhong(vec3 worldPos, vec3 viewDir, Light light, vec3 normal, vec3 albedo, vec3 reflectionColor)
{
	vec3 lightColor = light.color;
	vec3 lightDir = light.position - worldPos;
	float distance = length(lightDir);
	lightDir /= distance;
	float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

	vec3 halfDir = normalize(lightDir + viewDir);
	
	float diff = max(dot(normal, lightDir), 0.0f);
	vec3 diffuse = diff * lightColor * albedo;

	float spec = pow(max(dot(normal, halfDir), 0.0f), 128);
	vec3 specular = spec * lightColor;
	vec3 finalSpecular = mix(reflectionColor, specular, 1.0f - spec);

	return (diffuse + finalSpecular) * attenuation;
}

/** PBR Attempt 1 (UNUSED) **/
vec3 CalcSchlickFresnelReflectance(float hDotV, vec3 fresnel)
{
	return fresnel + (1.0f - fresnel) * pow(1.0f - hDotV, 5.0f);
}

vec3 CookTorrance(float nDotH, float hDotV, float roughness, float nDotL, float nDotV, vec3 fresnel)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;

	float geoTerm = (nDotL * nDotV) / (nDotL * (1.0f - alpha2) + alpha2);

	vec3 fresnelTerm = CalcSchlickFresnelReflectance(max(hDotV, 0.001f), fresnel);

	float normalDistGGX = alpha2 / (PI * pow((nDotH * nDotH) * (alpha2 - 1.0f) + 1.0f, 2.0f));

	return fresnelTerm * geoTerm * normalDistGGX;
}

vec3 ComputeLighting(vec3 albedo, vec3 normal, float metallic, float roughness, vec3 lightDir, vec3 viewDir)
{
	vec3 halfVect = normalize(lightDir + viewDir);
	
	float nDotL = max(dot(normal, lightDir), 0.001f);
	float nDotV = max(dot(normal, viewDir), 0.001f);
	float nDotH = max(dot(normal, halfVect), 0.001f);
	float hDotV = max(dot(halfVect, viewDir), 0.001f);
	vec3 fresnel = vec3(0.04f); // For dielectrics
	fresnel = mix(fresnel, albedo, metallic);

	vec3 cookTorrance = CookTorrance(nDotH, hDotV, roughness, nDotL, nDotV, fresnel);

	float diffuseReflection = 1.0f - metallic;
	vec3 diffuse = (1.0f - cookTorrance) * albedo / PI;

	return (diffuse + cookTorrance) * nDotL;
}


/** PBR Attempt 2 (UNUSED) **/
vec3 fresnelSchlick(float cosTheta, vec3 F0) 
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float ggx(float roughness, float cosTheta) 
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float cos2Theta = cosTheta * cosTheta;
    return (alpha2) / (3.141592 * pow(cos2Theta * (alpha2 - 1.0) + 1.0, 2.0));
}

vec3 PBR(vec3 worldPos, vec3 viewDir, vec3 lightDir, vec3 normal, vec3 albedo, float roughness, vec3 F0, vec3 radiance, vec3 reflectionColor)
{
    // Diffuse Lighting using Lambertian reflectance
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * albedo;

    // Specular Reflection using Cook-Torrance BRDF
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfwayDir), 0.0);
    float VdotH = max(dot(viewDir, halfwayDir), 0.0);

    // GGX distribution for roughness
    float D = ggx(roughness, NdotH);

    // Fresnel-Schlick approximation for specular reflection
    vec3 F = fresnelSchlick(VdotH, F0);
	vec3 finalReflection = reflectionColor * F;

    // Geometry function
    float G = min(1.0, min((2.0 * NdotH * max(dot(normal, viewDir), 0.0)) / VdotH, (2.0 * NdotH * max(dot(normal, lightDir), 0.0)) / VdotH));

    // Final specular reflection using Cook-Torrance BRDF
    vec3 specular = (D * F * G) / (4.0 * max(dot(normal, lightDir), 0.0) * max(dot(normal, viewDir), 0.0) + 0.001);

	return (diffuse + specular + finalReflection) * radiance;
}


/** PBR Attempt 3 **/
//Normal Distribution Function - How many microfacets have their half vector aligned with the surface normal, 
//								 aka how many reflect light back to the viewer
float GGXDistribution(float nDotH, float roughness)
{
	float alpha2 = roughness * roughness * roughness * roughness;
	float d = nDotH * nDotH * (alpha2 - 1.0f) + 1.0f;
	return alpha2 / (PI * d * d);
}

float GeomSmith(float dp, float roughness)
{
	float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
	float denom = dp * (1 - k) + k;
	return dp / denom;
}

// Probability that the microfacet with a given normal vector will be both visible to the light source and the viewer
// aka approximation of probability of self-shadowing between microfacets, high roughness = higher probability that microfacets will occlude neighbors
float SchlickGGX(float nDotV, float nDotL, float roughness)
{
	return GeomSmith(nDotV, roughness) * GeomSmith(nDotL, roughness);
}

// Fresnel reflection, occurs when light crosses the boundary between two materials,
// and part of the light is transmitted through the material, and the rest is reflected at the boundary.
// Light is transmitted more on surfaces with a normal with a smaller angle from the view direction, and reflected on surfaces
// with a normal that has a larger angle difference from the view direction
vec3 SchlickFresnel(float vDotH, vec3 F0) // F0 is how reflective the surface is when viewed from the normal direction, ex. water has a low F0, silver has a high F0
{
	return F0 + (1.0f - F0) * pow(clamp(1.0f - vDotH, 0.0f, 1.0f), 5);
}

vec3 CalcPBR(vec3 normal, vec3 viewDir, vec3 halfDir, vec3 lightDir, vec3 lightIntensity, vec3 fLambert, vec3 F0, float roughness, vec3 reflectionColor)
{
	float nDotH = max(dot(normal, halfDir), 0.0f);
	float vDotH = max(dot(viewDir, halfDir), 0.0f);
	float nDotL = max(dot(normal, lightDir), 0.0f);
	float nDotV = max(dot(normal, viewDir), 0.0f);

	vec3 fresnel = SchlickFresnel(vDotH, F0);
	vec3 kS = fresnel; // reflection index
	vec3 kD = 1.0f - kS; // refraction index

	vec3 finalReflection = reflectionColor * kS;
	
	// D * F * G
	vec3 specBRDFNom = GGXDistribution(nDotH, roughness) * fresnel * SchlickGGX(nDotV, nDotL, roughness);
	// 4 * nDotL * nDotV
	float specBRDFDenom = 4.0f * nDotL * nDotV + 0.0001f;
	vec3 specBRDF = specBRDFNom / specBRDFDenom;

	vec3 diffuseBRDF = kD * fLambert / PI;
	return (diffuseBRDF + specBRDF + finalReflection) * lightIntensity * nDotL;
}

void main() 
{
	uint colorIndex = materialIndex;
	uint metalRoughIndex = materialIndex + 1;
	uint normalIndex = materialIndex + 2;

	vec4 colorTexture = texture(textures[colorIndex], inUV);
	vec3 albedo = colorTexture.rgb * inColor * 
				imageMetaInfo[materialIndex / 3].constants.colorFactors.xyz;
	float roughness = texture(textures[metalRoughIndex], inUV).g *
				imageMetaInfo[materialIndex / 3].constants.metalRoughFactors.y;
	float metalness = texture(textures[metalRoughIndex], inUV).b *
				imageMetaInfo[materialIndex / 3].constants.metalRoughFactors.x;

	vec3 normal = normalize(inNormal);
	vec3 tangent = normalize(inTangent);
	vec3 biTangent = normalize(cross(normal, tangent) * inTangentW);

	mat3 TBN = mat3(tangent, biTangent, normal);

	vec4 normalMap = texture(textures[normalIndex], inUV);
	normalMap.rgb = (normalMap).rgb * 2.0f - 1.0f;
	if (normalMap.a > 0.0f)
	{
		normal = normalize(TBN * normalMap.rgb);
	}

	vec3 viewDir = normalize(sceneData.cameraPos - inWorldPos);

	vec3 reflectedDir = reflect(-viewDir, normal);
	vec3 reflectionColor = texture(cubeMap, reflectedDir).rgb;

	vec3 finalColor = vec3(0.0f);

	vec3 F0 = vec3(0.04); // Use this default value for dielectrics, base reflectivity
	F0 = mix(F0, albedo, metalness);

	vec3 fLambert = albedo;
	fLambert = mix(fLambert, vec3(0.0f), metalness);

	for (int i = 0; i < sceneData.numLights; i++)
	{
		Light light = lights[i];
		vec3 lightDir = light.position - inWorldPos;
		float distance = length(lightDir);
		if (distance > light.radius)
		{
			continue;
		}
		lightDir /= distance;

		vec3 halfDir = normalize(viewDir + lightDir);

		//float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance); // Only used for Blinn-Phong
		vec3 lightIntensity = (light.color * light.intensity) / (distance * distance);

		finalColor += CalcPBR(normal, viewDir, halfDir, lightDir, lightIntensity, fLambert, F0, roughness, reflectionColor);
	}

	vec3 sunDir = -normalize(sceneData.sunlightDirection).xyz;
	vec3 sunRadiance = sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;
	vec3 sunHalfDir = normalize(viewDir + sunDir);
	finalColor += CalcPBR(normal, viewDir, sunHalfDir, -normalize(sceneData.sunlightDirection).xyz, sunRadiance, fLambert, F0, roughness, reflectionColor); 
	vec3 ambient = albedo * sceneData.ambientColor.xyz;
	outFragColor = vec4(finalColor + ambient, 1.0f);
}