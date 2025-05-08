#version 450

#extension GL_GOOGLE_include_directive : require

layout(set = 0, binding = 0) uniform sampler2D hdrImage;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    float exposure;
	float renderScale;
} pushConstants;

void main() 
{
	const float gamma = 2.2f;
	vec3 hdrColor = texture(hdrImage, uv * pushConstants.renderScale).rgb;
	vec3 tonemapped = vec3(1.0f) - exp(-hdrColor * pushConstants.exposure);
	//vec3 tonemapped = hdrColor / (hdrColor + vec3(pushConstants.exposure));
	//tonemapped = pow(tonemapped, vec3(1.0f / gamma));

	outColor = vec4(tonemapped, 1.0f);
}