#version 450

#extension GL_GOOGLE_include_directive : require

#include "struct_definitions.glsl"

layout(set = 0, binding = 0) uniform sampler2D noiseTexture;
layout(set = 0, binding = 1) uniform KernelBuffer
{
	vec3 kernel[16];
};
layout(set = 0, binding = 2) uniform sampler2D positionTexture;
layout(set = 0, binding = 3) uniform sampler2D normalTexture;

layout(set = 1, binding = 0) uniform SceneDataBuffer{   
	SceneData sceneData;
};

layout(push_constant) uniform PushConstants
{
	vec2 screenResolution;
	float radius;
	float bias;
	uint kernelSize;
	float power;
} pushConstants; 

layout(location = 0) in vec2 inUV;
layout(location = 0) out float outAO;

void main() 
{
	//vec2 noiseTileScale = pushConstants.screenResolution / 4.0f;
	//vec2 noiseUV = (inUV / pushConstants.screenResolution) * noiseTileScale;
	vec2 noiseUV = inUV * 0.125f; // 8x8 noise texture

	vec3 normal = mat3(sceneData.view) * normalize(texture(normalTexture, inUV * sceneData.renderScale)).rgb; 
	vec3 randomVec = normalize(texture(noiseTexture, noiseUV).xyz);
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 biTangent = normalize(cross(normal, tangent));
	mat3 TBN = mat3(tangent, biTangent, normal);

	vec3 position = (sceneData.view * texture(positionTexture, inUV * sceneData.renderScale)).xyz; 

	float occlusion = 0.0f;
	for (uint i = 0; i < pushConstants.kernelSize; i++)
	{
		vec3 samplePos = TBN * kernel[i];
		samplePos = position + samplePos * pushConstants.radius;

		vec3 sampleDir = normalize(samplePos - position);
		float nDotS = max(dot(normal, sampleDir), 0.0f);

		vec4 offset = sceneData.proj * vec4(samplePos, 1.0f);
		offset.xyz /= offset.w;
		offset.xyz = offset.xyz * 0.5f + 0.5f;

		if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
		{
			continue;
		}

		float sampleDepth = (sceneData.view * texture(positionTexture, offset.xy * sceneData.renderScale)).z;

		float rangeCheck = smoothstep(0.0f, 1.0f, pushConstants.radius / abs(position.z - sampleDepth));
		occlusion += (sampleDepth >= (samplePos.z + pushConstants.bias) ? 1.0f : 0.0f) * rangeCheck * nDotS;
		//occlusion += rangeCheck * step(samplePos.z, sampleDepth) * nDotS;
	}
	occlusion = 1.0f - (occlusion / pushConstants.kernelSize);

	outAO = pow(occlusion, pushConstants.power);
}