#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "struct_definitions.glsl"

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform sampler2D positionColor;
layout (set = 0, binding = 1) uniform sampler2D normalColor;
layout (set = 0, binding = 2) uniform sampler2D albedoSpecColor;
layout (set = 0, binding = 3) uniform sampler2D metalRoughnessColor;

layout(std430, set = 0, binding = 4) readonly buffer Lights
{
	Light lights[];
};

layout (set = 0, binding = 5) uniform samplerCube cubeMap;
layout (set = 0, binding = 6) uniform sampler2D ssaoColor;
layout (set = 0, binding = 7) uniform sampler2D indirectColor;


layout(set = 1, binding = 0) uniform SceneDataBuffer{   
	SceneData sceneData;
};

layout(set = 2, binding = 0) uniform CascadeUBO
{
	mat4 lightSpaceTransform[NUM_CASCADES];
};

layout(set = 2, binding = 1) uniform sampler2D dirShadowMaps[];

layout(push_constant) uniform PushConstantData
{
	DeferredLightingConstants pushConstants;
};

const float PI = 3.14159265359f;

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
	float denom = dp * (1.0f - k) + k;
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

vec3 CalculateDirectLighting(vec3 worldPos, vec3 viewDir, vec3 normal, vec3 fLambert, vec3 F0, float roughness, vec3 reflectionColor)
{
	vec3 finalColor = vec3(0.0f);
	for (int i = 0; i < sceneData.numLights; i++)
	{
		Light light = lights[i];
		vec3 lightDir = light.position - worldPos;
		float distSq = max(dot(lightDir, lightDir), 1e-6);
		float inverseSq = inversesqrt(distSq);
		if ((distSq > (light.radius * light.radius)) || (inverseSq < 0.01f))
		{
			continue;
		}
		lightDir *= inverseSq;

		vec3 halfDir = normalize(viewDir + lightDir);

		vec3 lightIntensity = light.color * light.intensity * inverseSq;
		finalColor += CalcPBR(normal, viewDir, halfDir, lightDir, lightIntensity, fLambert, F0, roughness, reflectionColor);
	}
	return finalColor;
}

float CalculateShadows(vec3 projCoords, sampler2D dirShadowMap, float nDotL)
{
	float closestDepth = texture(dirShadowMap, projCoords.xy).r;
	float currentDepth = projCoords.z;
	if (currentDepth > 1.0f)
	{
		return 0.0f;
	}
	
	float pixelSize = length(fwidth(projCoords.xy)); 

	float mipLevel = log2(pixelSize * textureSize(dirShadowMap, 0).x);
	float shadow = 0.0f;
	vec2 texelSize = 1.0f / vec2(textureSize(dirShadowMap, 0));
	float bias = max(pushConstants.maxBiasFactor * (1.0f - nDotL), pushConstants.minBiasFactor);
	for (int x = -1; x <= 1; x++)
	{
		for (int y = -1; y <= 1; y++)
		{
			float pcfDepth = texture(dirShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += (currentDepth + bias) < pcfDepth ? 1.0f : 0.0f;
		}
	}
	shadow /= 9.0f;

	return shadow;
}

const vec2 cascadePlanes[4] = {
	vec2(0.1f, 10.0f),
	vec2(8.0f, 30.0f),
	vec2(22.5f, 100.0f),
	vec2(92.5f, 400.0f),
};

const uint cascadeResolutions[4] = { 4096, 4096, 4096, 4096 }; // Hardcoded resolutions...for now

int FindCascadeIndex(vec3 viewPos)
{
	float depthValue = -viewPos.z; // Don't forget to flip Z in view space as the camera points towards -Z
	
	int layer = -1;
	for (int i = 0; i < NUM_CASCADES; i++)
	{
		if (depthValue < cascadePlanes[i].y) // We're going from closest to farthest cascade, so first cascade that
		{									// the frag's depth value falls in will be our best cascade
			layer = i;
			return i;
		}
	}
	if (layer == -1)
	{
		return NUM_CASCADES - 1;
	}
		
}

void main() 
{
	vec4 positionSample = texture(positionColor, inUV * sceneData.renderScale);
	vec3 worldPos = positionSample.xyz;
	vec3 normal = texture(normalColor, inUV * sceneData.renderScale).rgb;
	vec4 albedoSpec = texture(albedoSpecColor, inUV * sceneData.renderScale);
	vec3 albedo = albedoSpec.rgb;
	vec4 metalRoughness = texture(metalRoughnessColor, inUV * sceneData.renderScale);
	float metalness = metalRoughness.b;
	float roughness = metalRoughness.g;

	vec3 viewDir = normalize(sceneData.cameraPos - worldPos);

	vec3 reflectedDir = reflect(-viewDir, normal);
	reflectedDir.z = -reflectedDir.z;
	vec3 reflectionColor = texture(cubeMap, reflectedDir).rgb * metalness * (1.0f - roughness);

	vec3 finalColor = vec3(0.0f);

	vec3 F0 = vec3(0.04); // Use this default value for dielectrics, base reflectivity
	F0 = mix(F0, albedo, metalness);

	vec3 fLambert = albedo;
	fLambert = mix(fLambert, vec3(0.0f), metalness);

	finalColor += CalculateDirectLighting(worldPos, viewDir, normal, fLambert, F0, roughness, reflectionColor);

	vec3 sunDir = -normalize(sceneData.sunlightDirection).xyz;
	vec3 sunRadiance = sceneData.sunlightColor.xyz * sceneData.sunlightColor.w;
	vec3 sunHalfDir = normalize(viewDir + sunDir);

	float occlusion = 1.0f;
	if (pushConstants.bDrawSsao == 1)
	{
		occlusion = texture(ssaoColor, inUV * sceneData.renderScale).r; 
	}
	
	float dirShadow = 0.0f;
	float indirectLight = 0.0f;
	if (pushConstants.bDrawShadowMaps == 1)
	{
		vec3 viewPos = (sceneData.view * vec4(worldPos, 1.0f)).xyz;
		int cascadeIndex = FindCascadeIndex(viewPos);
		vec4 lightSpacePos = lightSpaceTransform[cascadeIndex] * vec4(worldPos, 1.0f);
		vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
		projCoords.xy = projCoords.xy * 0.5f + 0.5f;

		float closerShadow = CalculateShadows(projCoords, dirShadowMaps[cascadeIndex], dot(normal, sunDir));

		dirShadow = closerShadow;
		if (cascadeIndex < NUM_CASCADES - 1)
		{
			int nextCascadeIndex = cascadeIndex + 1;
			float fadeStart = cascadePlanes[nextCascadeIndex].x;
			float fadeEnd = cascadePlanes[cascadeIndex].y;
			float depth = -viewPos.z;
			if (depth > fadeStart && depth <= fadeEnd)
			{
				vec4 nextLightSpacePos = lightSpaceTransform[nextCascadeIndex] * vec4(worldPos, 1.0f);
				vec3 nextProjCoords = nextLightSpacePos.xyz / nextLightSpacePos.w;
				nextProjCoords.xy = nextProjCoords.xy * 0.5f + 0.5f;
				float fartherShadow = CalculateShadows(nextProjCoords, dirShadowMaps[nextCascadeIndex], dot(normal, sunDir));

				float blend = smoothstep(fadeStart, fadeEnd, depth); 
				dirShadow = mix(closerShadow, fartherShadow, blend);
			}
		}

		if (pushConstants.bDrawReflectiveShadowMaps == 1)
		{
			finalColor += texture(indirectColor, inUV * sceneData.renderScale).rgb * occlusion;
		}
	}
	vec3 ambient = albedo * sceneData.ambientColor.xyz * pushConstants.ambientScale;
	finalColor += CalcPBR(normal, viewDir, sunHalfDir, sunDir, sunRadiance, fLambert, F0, roughness, reflectionColor) * (1.0f - dirShadow); 
	outFragColor = vec4(finalColor + (ambient * occlusion), 1.0f);
}