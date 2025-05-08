#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

vec2 positions[3] = vec2[](
	vec2(-1.0f, -1.0f),
	vec2( 3.0f, -1.0f),
	vec2(-1.0f,  3.0f) 
);

layout(location = 0) out vec2 outUV;

void main() 
{
	outUV = (positions[gl_VertexIndex] + 1.0f) * 0.5f; // Convert from clip space [-1,1] to texture UV coords [0, 1]

	gl_Position = vec4(positions[gl_VertexIndex], 0.0f, 1.0f);
}