#version 450

#extension GL_GOOGLE_include_directive : require

layout(set = 0, binding = 0) uniform samplerCube skybox;

layout(location = 0) in vec3 inWorldPos;
layout(location = 0) out vec4 outColor;

void main() 
{
	vec3 direction = normalize(inWorldPos);
	direction.z = -direction.z;
	vec3 color = texture(skybox, direction).rgb;

	outColor = vec4(color, 1.0f);
}