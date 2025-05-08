#version 450

#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFiltered;

layout(set = 0, binding = 0) uniform sampler2D indirectColor;
layout(set = 0, binding = 1) uniform sampler2D positionColor;
layout(set = 0, binding = 2) uniform sampler2D normalColor;

layout(push_constant) uniform PushConstants 
{
    float stepWidth;     // Grows each iteration (1.0, 2.0, 4.0, etc.)
    float phiColor;      // Controls sensitivity to color difference
    float phiNormal;     // Controls sensitivity to normal difference
    float phiPosition;   // Controls sensitivity to depth difference
    vec2 texelSize;      // 1.0 / resolution
} pushConstants;

const ivec2 kernel[5] = ivec2[](
    ivec2( 0,  0),
    ivec2( 1,  0),
    ivec2( -1, 0),
    ivec2( 0,  1),
    ivec2( 0, -1)
);

const float weights[5] = float[](
    0.4, 0.15, 0.15, 0.15, 0.15
);

void main() {
    vec3 centerColor = texture(indirectColor, inUV).rgb;
    vec3 centerNormal = texture(normalColor, inUV).xyz;
    float centerPos = texture(positionColor, inUV).r;

    vec3 result = vec3(0.0);
    float totalWeight = 0.0;

    for (int i = 0; i < 5; i++)
    {
        vec2 offsetUV = inUV + pushConstants.texelSize * pushConstants.stepWidth * vec2(kernel[i]);
        vec3 sampleColor = texture(indirectColor, offsetUV).rgb;
        vec3 sampleNormal = texture(normalColor, offsetUV).xyz;
        float samplePos = texture(positionColor, offsetUV).r;

        float weightColor = exp(-dot(sampleColor - centerColor, sampleColor - centerColor) / pushConstants.phiColor);
        float weightNormal = exp(-pow(max(0.0, 1.0 - dot(sampleNormal, centerNormal)), 2.0) / pushConstants.phiNormal);
        float weightPos = exp(-pow(length(samplePos - centerPos), 2.0) / pushConstants.phiPosition);

        float weight = weights[i] * weightColor * weightNormal * weightPos;

        result += sampleColor * weight;
        totalWeight += weight;
    }

    /*for (int i = 0; i < 5; ++i) {
        ivec2 offset = kernel[i] * int(pushConstants.stepWidth);
        vec2 offsetUV = inUV + vec2(offset) * pushConstants.texelSize;

        vec3 sampleColor  = texture(indirectColor, offsetUV).rgb;
        vec3 samplePos    = texture(positionColor, offsetUV).xyz;
        vec3 sampleNormal = texture(normalColor, offsetUV).xyz;

        float posDiff = length(centerPos - samplePos);
        float posWeight = exp(-posDiff * pushConstants.phiPosition);

        float normalDiff = max(0.0, dot(centerNormal, sampleNormal));
        float normalWeight = exp(-(1.0 - normalDiff) * pushConstants.phiNormal);

        float weight = posWeight * normalWeight;

        result += sampleColor * weight;
        totalWeight += weight;
    }*/

    /*for (int i = -2; i <= 2; ++i) 
    {
        vec2 offsetUV = inUV + vec2(float(i * pushConstants.stepWidth)) * pushConstants.texelSize;
        vec3 sampleColor = texture(indirectColor, offsetUV).rgb;
        vec3 samplePos = texture(positionColor, offsetUV).xyz;
        vec3 sampleNormal = texture(normalColor, offsetUV).xyz;

        float posWeight = exp(-length(samplePos - centerPos) * 50.0);
        float normalWeight = pow(max(dot(centerNormal, sampleNormal), 0.0), 32.0);

        float weight = kernel[i + 2] * posWeight * normalWeight;

        result += sampleColor * weight;
        weightSum += weight;
    }*/

    outFiltered = vec4(result / max(totalWeight, 1e-4), 1.0f);
}