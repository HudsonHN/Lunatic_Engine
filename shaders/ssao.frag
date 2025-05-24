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
	vec2 noiseUV = inUV * 0.125f; // 8x8 noise texture

	vec3 normal = mat3(sceneData.view) * normalize(texture(normalTexture, inUV * sceneData.renderScale)).rgb; 
	vec3 randomVec = normalize(texture(noiseTexture, noiseUV).xyz);
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 biTangent = normalize(cross(normal, tangent));
	mat3 TBN = mat3(tangent, biTangent, normal); // Tangent to view-space matrix

	vec3 position = (sceneData.view * texture(positionTexture, inUV * sceneData.renderScale)).xyz; 

	float occlusion = 0.0f;
	uint validSamples = 0;
	for (uint i = 0; i < pushConstants.kernelSize; i++)
	{
		vec3 samplePos = TBN * kernel[i]; // Transform a random hermispherical direction to view space
		samplePos = position + samplePos * pushConstants.radius; // Add random direction to the current point + desired radius to get the sample point within hemisphere

		vec3 sampleDir = normalize(samplePos - position);
		float nDotS = max(dot(normal, sampleDir), 0.0f); // Importance weight dependent on how different the random point's direction is from the original point's normal
		nDotS = clamp(nDotS, 0.1f, 1.0f);

		vec4 offset = sceneData.proj * vec4(samplePos, 1.0f); // Clip space transform
		offset.xyz /= offset.w; // Normalization to NDC
		offset.xyz = offset.xyz * 0.5f + 0.5f; // Get the UV coordinate of the random point to sample the depth of the actual geometry that exists there

		if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
		{
			continue;
		}
		validSamples++;

		float sampleDepth = (sceneData.view * texture(positionTexture, offset.xy * sceneData.renderScale)).z;

		float rangeCheck = smoothstep(0.0f, 1.0f, pushConstants.radius / abs(position.z - sampleDepth));
		float bias = pushConstants.bias * (1.0f + position.z * 0.01f);
		occlusion += (sampleDepth >= (samplePos.z + bias) ? 1.0f : 0.0f) * rangeCheck * nDotS;
		//occlusion += rangeCheck * step(samplePos.z, sampleDepth) * nDotS;
	}
	occlusion = validSamples != 0 ? (1.0f - (occlusion / validSamples)) : 1.0f;

	outAO = clamp(pow(occlusion, pushConstants.power), 0.0f, 1.0f);
}