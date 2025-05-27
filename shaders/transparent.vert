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
layout (location = 7) out vec3 outViewPos;
layout (location = 8) out vec4 outLightSpacePos[NUM_CASCADES];

layout(std430, set = 0, binding = 0) readonly buffer SurfaceMetaInfoBuffer 
{
	SurfaceInfo surfaces[];
};

layout(set = 1, binding = 0) uniform CascadeUBO
{
	mat4 lightSpaceTransform[NUM_CASCADES];
} ubo;

layout(set = 2, binding = 0) uniform SceneDataBuffer{   
	SceneData sceneData;
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

	vec4 worldPosition = surface.worldTransform * position;

	outViewPos = (sceneData.view * worldPosition).xyz;
	outWorldPos = worldPosition.xyz;
	outMaterialIndex = surface.materialIndex;
	outNormal = (surface.worldTransform * vec4(v.normal, 0.0f)).xyz;
	outColor = v.color.xyz;	
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outTangent = (surface.worldTransform * vec4(v.tangent.xyz, 0.0f)).xyz;
	outTangentW = v.tangent.w;
	for (int i = 0; i < NUM_CASCADES; i++)
	{
		outLightSpacePos[i] = ubo.lightSpaceTransform[i] * worldPosition;
	}
	
	vec4 clipPos = (sceneData.viewProj * worldPosition);
	clipPos.xy += float(sceneData.bApplyTAA) * sceneData.jitterOffset * clipPos.w;
	gl_Position = clipPos;
}