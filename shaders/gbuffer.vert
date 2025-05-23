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

layout(std430, set = 0, binding = 1) readonly buffer VertexBuffer
{ 
	Vertex vertices[];
};

layout(set = 1, binding = 0) uniform PrevSceneDataBuffer{   
	SceneData prevSceneData;
};
layout(set = 1, binding = 1) uniform CurrSceneDataBuffer{   
	SceneData currSceneData;
};

void main() 
{
	SurfaceInfo surface = surfaces[gl_InstanceIndex];

	Vertex v = vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);
	//debugPrintfEXT("gl_InstanceIndex: %d, gl_VertexIndex: %d, X: %f, Y: %f, Z: %f", gl_InstanceIndex, gl_VertexIndex, v.position.x, v.position.y, v.position.z);

	vec4 worldPosition = surface.worldTransform * position;

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