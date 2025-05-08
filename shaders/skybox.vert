#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

vec2 positions[3] = vec2[](
	vec2(-1.0f, -1.0f),
	vec2( 3.0f, -1.0f),
	vec2(-1.0f,  3.0f) 
);

layout(location = 0) out vec3 outWorldPos;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
} pushConstants;


void main() 
{
	mat4 invViewProj = inverse(pushConstants.viewProj);
	outWorldPos = (invViewProj * vec4(positions[gl_VertexIndex], 0.0f, 1.0f)).xyz;
	gl_Position = vec4(positions[gl_VertexIndex], 0.0f, 1.0f);
}