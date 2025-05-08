#version 450

#extension GL_GOOGLE_include_directive : require

layout(set = 0, binding = 0) uniform sampler2D image;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstant
{
    float screenWidth;
    float blurAmount;
    float renderScale;
} pushConstant; 

const float pixelKernel[9] =
{
    -4.0,
    -3.0,
    -2.0,
    -1.0,
     0.0,
     1.0,
     2.0,
     3.0,
     4.0
};
 
const float blurWeights[9] =
{
    0.02242,
    0.04036,
    0.12332,
    0.20179,
    0.22422,
    0.20179,
    0.12332,
    0.04036,
    0.02242
};

void main() 
{
	vec4 color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    float texelSize = 1.0 / pushConstant.screenWidth * pushConstant.renderScale;

    for(int i = 0; i < 9; i++)
    {
   	    color += texture(image, (inUV * pushConstant.renderScale) + vec2(texelSize * pixelKernel[i] * pushConstant.blurAmount, 0.0f)) * blurWeights[i];
    }

    color.a = 1.0;

	outColor = color;
}