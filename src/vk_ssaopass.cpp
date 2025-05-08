#include "vk_ssaopass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"
#include <random>

void SSAOPass::DataSetup(LunaticEngine* engine)
{
    ssaoImageHandle = &engine->_ssaoColor;
    noiseImageHandle = &engine->_noiseAOTexture;
    kernelBufferHandle = &engine->_kernelBuffer;
    
    positionColorHandle = &engine->_positionColor;
    normalColorHandle = &engine->_normalColor;
}

void SSAOPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    ssaoDescriptorSetLayout = builder.Build(engine->_device);
    deletionQueue.PushFunction([=]() {
        vkDestroyDescriptorSetLayout(engine->_device, ssaoDescriptorSetLayout, nullptr);
        });
    ssaoDescriptorSet = descriptorAllocator->Allocate(engine->_device, ssaoDescriptorSetLayout);

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = ssaoDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerRepeat;
        bindingInfo.isImage = true;

        noiseImageHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = ssaoDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindingInfo.isImage = false;

        kernelBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = ssaoDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        positionColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 3;
        bindingInfo.descriptorSet = ssaoDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        normalColorHandle->bindingInfos.push_back(bindingInfo);
    }
}

void SSAOPass::PipelineSetup(LunaticEngine* engine)
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts =
    {
        ssaoDescriptorSetLayout,
        sceneDataDescriptorSetLayout
    };

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(SSAOConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.pSetLayouts = descriptorSetLayouts.data();
    layoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    layoutInfo.pPushConstantRanges = &pushConstant;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &pipelineLayout));

    VkShaderModule vertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\ssao.vert.spv", engine->_device, &vertexShader))
    {
        fmt::print("Error when building the SSAO vert shader, cannot load file.\n");
    }

    VkShaderModule fragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\ssao.frag.spv", engine->_device, &fragShader))
    {
        fmt::print("Error when building the SSAO frag shader, cannot load file.\n");
    }

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(vertexShader, fragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(VK_FORMAT_R8_UNORM);
    pipelineBuilder.disable_depthtest();
    pipelineBuilder.write_outside_stencil();

    pipelineBuilder._pipelineLayout = pipelineLayout;

    pipeline = pipelineBuilder.build_pipeline(engine->_device, 1);

    vkDestroyShaderModule(engine->_device, vertexShader, nullptr);
    vkDestroyShaderModule(engine->_device, fragShader, nullptr);

    deletionQueue.PushFunction([=]()
        {
            vkDestroyPipeline(engine->_device, pipeline, nullptr);
            vkDestroyPipelineLayout(engine->_device, pipelineLayout, nullptr);
        });
}

void SSAOPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    AllocatedImage& ssaoImage = renderGraph->GetImage(*ssaoImageHandle);

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(ssaoImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(engine->_drawExtent, &colorAttachment, nullptr, nullptr);

    std::vector<VkDescriptorSet> descriptorSets =
    {
        ssaoDescriptorSet,
        sceneDataDescriptorSet
    };

    engine->_ssaoConstants.screenResolution = { static_cast<float>(engine->_drawExtent.width), static_cast<float>(engine->_drawExtent.height) };

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SSAOConstants), &engine->_ssaoConstants);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

    // Set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(engine->_drawExtent.width);
    viewport.height = static_cast<float>(engine->_drawExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = engine->_drawExtent.width;
    scissor.extent.height = engine->_drawExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);
    ++engine->_perfStats.drawcallCount;
}