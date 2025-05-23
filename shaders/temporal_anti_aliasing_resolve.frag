#version 450

#extension GL_GOOGLE_include_directive : require

#include "struct_definitions.glsl"
#include "helper_functions.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outVelocity;

layout(set = 0, binding = 0) uniform sampler2D previousFrame;
layout(set = 0, binding = 1) uniform sampler2D currentFrame;
layout(set = 0, binding = 2) uniform sampler2D currentWorldPosImage;
layout(set = 0, binding = 3) uniform sampler2D prevVelocityImage;

layout(set = 1, binding = 0) uniform PrevSceneData {
	SceneData prevSceneData;
};

layout(set = 1, binding = 1) uniform CurrSceneData {
	SceneData currSceneData;
};

vec4 SampleTextureCatmullRom(sampler2D tex, vec2 uv)
{
	vec2 texSize = textureSize(tex, 0);
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    vec2 samplePos = uv * texSize;
    vec2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    vec2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    vec2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    vec2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    vec2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    vec2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    vec2 texPos0 = texPos1 - 1;
    vec2 texPos3 = texPos1 + 2;
    vec2 texPos12 = texPos1 + offset12;

    texPos0 /= texSize;
    texPos3 /= texSize;
    texPos12 /= texSize;

    vec4 result = vec4(0.0f);
    result += textureLod(tex, vec2(texPos0.x, texPos0.y), 0.0f) * w0.x * w0.y;
    result += textureLod(tex, vec2(texPos12.x, texPos0.y), 0.0f) * w12.x * w0.y;
    result += textureLod(tex, vec2(texPos3.x, texPos0.y), 0.0f) * w3.x * w0.y;

    result += textureLod(tex, vec2(texPos0.x, texPos12.y), 0.0f) * w0.x * w12.y;
    result += textureLod(tex, vec2(texPos12.x, texPos12.y), 0.0f) * w12.x * w12.y;
    result += textureLod(tex, vec2(texPos3.x, texPos12.y), 0.0f) * w3.x * w12.y;

    result += textureLod(tex, vec2(texPos0.x, texPos3.y), 0.0f) * w0.x * w3.y;
    result += textureLod(tex, vec2(texPos12.x, texPos3.y), 0.0f) * w12.x * w3.y;
    result += textureLod(tex, vec2(texPos3.x, texPos3.y), 0.0f) * w3.x * w3.y;

    return result;
}

vec2 CalcVelocity(mat4 prevViewProj, mat4 currViewProj, vec2 uv, vec2 prevJitter, vec2 currJitter)
{
	vec4 currentWorldPos = texture(currentWorldPosImage, uv);
	vec4 projPrevPos = prevViewProj * currentWorldPos; // Clip space
	projPrevPos.xy -= prevJitter * projPrevPos.w; // Unjitter in clip space
	projPrevPos.xyz /= projPrevPos.w; // Convert to NDC space
	projPrevPos.xy = (projPrevPos.xy * 0.5f) + 0.5f; // Scale to UV coords [0,1]

	vec4 projCurrPos = currViewProj * currentWorldPos; // Clip space
	projCurrPos.xyz /= projCurrPos.w; // Convert to NDC space, no need to unjitter
	projCurrPos.xy = (projCurrPos.xy * 0.5f) + 0.5f; // Scale to UV coords [0,1]
	return (projCurrPos - projPrevPos).xy;
}

const vec3 tentWeights[3] = vec3[3](
    vec3(0.0625f, 0.125f, 0.0625f),
    vec3(0.125f,  0.25f,  0.125f),
    vec3(0.0625f, 0.125f, 0.0625f)
);

const float lumaSigma = 8.0f; // Lower value = smoother blending, higher value = more aggressive rejection

// Sample neighbors in a box and weigh their contributions according to the tent weights above and the luma weight between center pixel and neighbors
// also get the component-wise min/max color values for clamping the previous frame to values of the current frame
vec3 SampleNeighborhoodMinMax(ivec2 texelUV, ivec2 resolution, vec3 currentColor, sampler2D currentFrame, out vec3 minCol, out vec3 maxCol)
{
	minCol = currentColor;
	maxCol = currentColor;
	float centerLuma = luma(currentColor);
	vec3 sampleOutput = vec3(0.0f);
	for (int x = -1; x <= 1; x++)
	{
		for (int y = -1; y <= 1; y++)
		{
			ivec2 finalUV = texelUV + ivec2(x, y);
			finalUV = clamp(finalUV, ivec2(0), resolution - ivec2(1));
			vec3 color = texelFetch(currentFrame, finalUV, 0).rgb;
			float lumaDiff = abs(luma(color) - centerLuma);
			float lumaWeight = exp(-lumaDiff * lumaSigma); // Exponential luminance falloff, e^x | e^-x = (1 / e^x), 
															// so a higher lumaDiff and lumaSigma = smaller contribution
			float weight = tentWeights[x + 1][y + 1] * lumaWeight; // Add +1 to index accessors to change from -1:1 to 0:2
			sampleOutput += color * weight; 
			minCol = min(minCol, color);
			maxCol = max(maxCol, color);
		}
	}
	return sampleOutput;
}

void main() 
{
	vec2 prevVelocity = texture(prevVelocityImage, inUV * prevSceneData.renderScale).xy;
	vec2 currVelocity = CalcVelocity(prevSceneData.viewProj, currSceneData.viewProj, inUV * currSceneData.renderScale, prevSceneData.jitterOffset, currSceneData.jitterOffset);

	float velocityLength = length(prevVelocity - currVelocity);
	float velocityDisocclusion = clamp((velocityLength - 0.001f) * 10.0f, 0.0f, 1.0f);

	ivec2 texSize = textureSize(previousFrame, 0);
	vec2 pixelMargin = 1.5f / vec2(texSize); // Use pixel margin to properly clamp UV, compensating for catmull rom sampling
	vec2 prevUV = clamp(inUV - prevVelocity, pixelMargin, vec2(1.0f) - pixelMargin); // The true position of the previous pixel 

	vec3 prevColor = SampleTextureCatmullRom(previousFrame, prevUV * prevSceneData.renderScale).xyz;
	vec3 currentColor = texture(currentFrame, inUV * currSceneData.renderScale).xyz;

	ivec2 resolution = textureSize(currentFrame, 0);
	ivec2 texelUV = ivec2(inUV * currSceneData.renderScale * resolution);

	vec3 boxMin = vec3(1.0f);
	vec3 boxMax = vec3(0.0f);
	vec3 sampledColor = SampleNeighborhoodMinMax(texelUV, resolution, currentColor, currentFrame, boxMin, boxMax);

	prevColor = clamp(prevColor, boxMin, boxMax);
	vec3 accumulation = prevColor * 0.9f + sampledColor * 0.1f;

	vec4 finalColor = vec4(mix(accumulation, sampledColor, velocityDisocclusion), 1.0f);
	outColor = finalColor;
	//outColor = vec4(mix(prevColor, currentColor, 0.1f), 1.0f);
	outVelocity = currVelocity;
}