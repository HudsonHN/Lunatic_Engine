#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_debug_printf : enable

#include "struct_definitions.glsl"

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec2 outUV;
layout (location = 4) out vec3 outTangent;
layout (location = 5) out flat float outTangentW;
layout (location = 6) out flat uint outMaterialIndex;
layout (location = 7) out vec4 outCurrClipPos;
layout (location = 8) out vec4 outPrevClipPos;

layout(std430, set = 0, binding = 0) readonly buffer SurfaceMetaInfoBuffer 
{
	SurfaceInfo surfaces[];
};

layout(set = 1, binding = 0) uniform PrevSceneDataBuffer
{   
	SceneData prevSceneData;
};
layout(set = 1, binding = 1) uniform CurrSceneDataBuffer
{   
	SceneData currSceneData;
};

layout(set = 2, binding = 0) readonly buffer BoneTransformBuffer
{   
	mat4 boneTransforms[];
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
	Vertex vertices[];
};

layout(push_constant) uniform PushConstants
{
	VertexBuffer vertexBuffer;
};

void main() 
{
	SurfaceInfo surface = surfaces[gl_InstanceIndex];

	Vertex v = vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	mat4 skinTransform = mat4(0.0f);
	for (int i = 0; i < MAX_BONE_INFLUENCES; i++)
	{
		uint boneIndex = v.boneIndices[i];
		// Bone transforms are all in a global buffer, accessed by their byte offset (surface.boneOffset) and boneIndex (v.boneIndices[i]) from 0 - 3
		skinTransform += v.boneWeights[i] * boneTransforms[i + surface.boneOffset + boneIndex];
	}

	vec4 worldPosition = surface.worldTransform * skinTransform * position;

	outWorldPos = worldPosition.xyz;
	outMaterialIndex = surface.materialIndex;
	outNormal = (surface.worldTransform * vec4(v.normal, 0.0f)).xyz;
	outColor = v.color.xyz;	
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outTangent = (surface.worldTransform * vec4(v.tangent.xyz, 0.0f)).xyz;
	outTangentW = v.tangent.w;
	
	vec4 clipPos = currSceneData.viewProj * worldPosition;
	outCurrClipPos = clipPos;
	outPrevClipPos = prevSceneData.viewProj * worldPosition;

	if (currSceneData.bApplyTAA == 1)
	{
		clipPos.xy += currSceneData.jitterOffset * clipPos.w;
	}
	gl_Position = clipPos;
}