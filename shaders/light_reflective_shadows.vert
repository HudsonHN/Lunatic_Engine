#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_debug_printf : enable

#include "struct_definitions.glsl"

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out float outTangentW;
layout(location = 5) out flat uint materialIndex;
layout(location = 6) out vec3 outColor;

layout(std430, set = 0, binding = 0) readonly buffer SurfaceMetaInfoBuffer 
{
	SurfaceInfo surfaces[];
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
	Vertex vertices[];
};

layout(push_constant) uniform PushConstants
{
	mat4 lightSpaceTransform;
	VertexBuffer vertexBuffer;
} pushConstant;

void main() 
{
	SurfaceInfo surface = surfaces[gl_InstanceIndex];

	Vertex v = pushConstant.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);
	vec4 normal = vec4(v.normal, 0.0f);
	vec4 tangent = vec4(v.tangent.xyz, 0.0f);

	vec4 worldPosition = surface.worldTransform * position;

	outColor = v.color.xyz;
	outWorldPos = worldPosition.xyz;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;
	outNormal = (surface.worldTransform * normal).xyz;
	outTangent = (surface.worldTransform * vec4(v.tangent.xyz, 0.0f)).xyz;
	outTangentW = v.tangent.w;
	materialIndex = surface.materialIndex;
	gl_Position = pushConstant.lightSpaceTransform * worldPosition;
}