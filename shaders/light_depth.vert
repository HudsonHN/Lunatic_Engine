#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_debug_printf : enable

#include "struct_definitions.glsl"

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

	vec4 worldPosition = surface.worldTransform * position;
	gl_Position = pushConstant.lightSpaceTransform * worldPosition;
}