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
layout (location = 7) in vec4 inCurrClipPos;
layout (location = 8) in vec4 inPrevClipPos;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedoSpec;
layout (location = 3) out vec4 outMetalRough;
layout (location = 4) out vec2 outVelocity;

layout(std430, set = 0, binding = 1) readonly buffer ImageMetaInfoBuffer
{   
	ImageMetaInfo imageMetaInfo[];
};

layout(set = 0, binding = 2) uniform sampler2D textures[];

layout(set = 1, binding = 0) uniform PrevSceneDataBuffer{   
	SceneData prevSceneData;
};
layout(set = 1, binding = 1) uniform CurrSceneDataBuffer{   
	SceneData currSceneData;
};

const mat4 bayerMatrix = mat4(
    vec4( 0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0 ),
    vec4(12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0 ),
    vec4( 3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0 ),
    vec4(15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0 )
);

const float PI = 3.14159265359f;
const float radius = 0.5f;

vec2 CalcVelocity(vec3 worldPos, mat4 prevViewProj, mat4 currViewProj, vec2 uv, vec2 prevJitter, vec2 currJitter)
{
	vec4 currentWorldPos = vec4(worldPos, 1.0f);
	vec4 projPrevPos = prevViewProj * currentWorldPos; // Clip space
	projPrevPos.xy -= prevJitter * projPrevPos.w; // Unjitter in clip space
	projPrevPos.xyz /= projPrevPos.w; // Convert to NDC space
	projPrevPos.xy = (projPrevPos.xy * 0.5f) + 0.5f; // Scale to UV coords [0,1]

	vec4 projCurrPos = currViewProj * currentWorldPos; // Clip space
	projCurrPos.xyz /= projCurrPos.w; // Convert to NDC space, no need to unjitter
	projCurrPos.xy = (projCurrPos.xy * 0.5f) + 0.5f; // Scale to UV coords [0,1]
	return (projCurrPos - projPrevPos).xy;
}

void main() 
{
	uint colorIndex = materialIndex;
	uint metalRoughIndex = materialIndex + 1;
	uint normalIndex = materialIndex + 2;

	vec4 colorTexture = texture(textures[colorIndex], inUV);

	ivec2 pixelCoord = ivec2(gl_FragCoord.xy) % 4;
	float threshold = bayerMatrix[pixelCoord.y][pixelCoord.x];
	if (colorTexture.a < threshold)
	{
		discard;
	}
	vec3 albedo = colorTexture.rgb * inColor * 
				imageMetaInfo[materialIndex / 3].constants.colorFactors.xyz;
	float metalness = texture(textures[metalRoughIndex], inUV).b *
				imageMetaInfo[materialIndex / 3].constants.metalRoughFactors.x;
	float roughness = texture(textures[metalRoughIndex], inUV).g *
				imageMetaInfo[materialIndex / 3].constants.metalRoughFactors.y;

	vec3 normal = normalize(inNormal);
	vec3 tangent = normalize(inTangent);
	vec3 biTangent = normalize(cross(normal, tangent) * inTangentW);

	mat3 TBN = mat3(tangent, biTangent, normal);

	vec4 normalMap = texture(textures[normalIndex], inUV);
	if (normalMap.rgb != vec3(1.0f, 1.0f, 1.0f) && normalMap.a > 0.0f)
	{
		normalMap.rgb = (normalMap).rgb * 2.0f - 1.0f;
		normal = normalize(TBN * normalMap.rgb);
	}

	outPosition = vec4(inWorldPos, 1.0f);
	outNormal = vec4(normal, 1.0f);
	outAlbedoSpec = vec4(albedo, 1.0f);
	outMetalRough = vec4(0.0f, roughness, metalness, 1.0f);

	vec3 currPosNDC = inCurrClipPos.xyz / inCurrClipPos.w;
	currPosNDC.xy = currPosNDC.xy * 0.5f + 0.5f;
	vec3 prevPosNDC = inPrevClipPos.xyz / inPrevClipPos.w;
	prevPosNDC.xy = prevPosNDC.xy * 0.5f + 0.5f;
	outVelocity = prevPosNDC.xy - currPosNDC.xy;
}