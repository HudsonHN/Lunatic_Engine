#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "struct_definitions.glsl"

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in float inTangentW;
layout(location = 5) in flat uint materialIndex;
layout(location = 6) in vec3 inColor;

layout(location = 0) out vec4 outWorldPos;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outFlux;

layout(std430, set = 0, binding = 1) readonly buffer ImageMetaInfoBuffer
{   
	ImageMetaInfo imageMetaInfo[];
};

layout(set = 0, binding = 2) uniform sampler2D textures[];

layout(set = 1, binding = 0) uniform SceneDataBuffer
{
	SceneData sceneData;
};

const float PI = 3.14159265359f;

const mat4 bayerMatrix = mat4(
    vec4( 0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0 ),
    vec4(12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0 ),
    vec4( 3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0 ),
    vec4(15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0 )
);

void main() 
{
	uint colorIndex = materialIndex;
	uint metalRoughIndex = materialIndex + 1;
	uint normalIndex = materialIndex + 2;

	ivec2 pixelCoord = ivec2(gl_FragCoord.xy) % 4;
	float threshold = bayerMatrix[pixelCoord.y][pixelCoord.x];

	vec4 colorTexture = texture(textures[colorIndex], inUV);

	vec3 albedo = colorTexture.rgb * inColor * 
				imageMetaInfo[materialIndex / 3].constants.colorFactors.xyz;
	if(colorTexture.a < threshold)
	{
		discard;
	}

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

	vec3 flux = sceneData.sunlightColor.rgb * sceneData.sunlightColor.a * albedo.rgb * dot(normal, -normalize(sceneData.sunlightDirection.xyz)) / PI;

	outWorldPos = vec4(inWorldPos, 1.0f);
	outNormal = vec4(normal, 0.0f);
	outFlux = vec4(flux, 1.0f);
}