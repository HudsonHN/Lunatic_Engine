#include "vk_atrousfilterpass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"
#include "vk_debug.h"

void AtrousFilterPass::DataSetup(LunaticEngine* engine)
{
    denoiseImageHandle = &engine->_indirectLightColor;
    positionColorHandle = &engine->_positionColor;
    normalColorHandle = &engine->_normalColor;
}

void AtrousFilterPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    atrousFilterDescriptorSetLayout = builder.Build(engine->_device);
    deletionQueue.PushFunction([=]()
        {
            vkDestroyDescriptorSetLayout(engine->_device, atrousFilterDescriptorSetLayout, nullptr);
        });
    atrousFilterDescriptorSet = descriptorAllocator->Allocate(engine->_device, atrousFilterDescriptorSetLayout);

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = atrousFilterDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        positionColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = atrousFilterDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        normalColorHandle->bindingInfos.push_back(bindingInfo);
    }
}

void AtrousFilterPass::PipelineSetup(LunaticEngine* engine)
{
    VkShaderModule fragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\atrous_filter.frag.spv", engine->_device, &fragShader)) {
        fmt::println("Error when building the atrous filter fragment shader module");
    }

    VkShaderModule vertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\atrous_filter.vert.spv", engine->_device, &vertexShader)) {
        fmt::println("Error when building the atrous filter shader module");
    }

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.size = sizeof(AtrousFilterConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &atrousFilterDescriptorSetLayout;
    layoutInfo.pPushConstantRanges = &pushConstantRange;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &pipelineLayout));

    deletionQueue.PushFunction([=]() {
        vkDestroyPipelineLayout(engine->_device, pipelineLayout, nullptr);
        });

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(vertexShader, fragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(VK_FORMAT_R16G16B16A16_SFLOAT);
    pipelineBuilder._pipelineLayout = pipelineLayout;

    pipeline = pipelineBuilder.build_pipeline(engine->_device, 1);

    deletionQueue.PushFunction([=]()
        {
            vkDestroyPipeline(engine->_device, pipeline, nullptr);
        });

    vkDestroyShaderModule(engine->_device, fragShader, nullptr);
    vkDestroyShaderModule(engine->_device, vertexShader, nullptr);
}

void AtrousFilterPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    AllocatedImage& denoiseImage = renderGraph->GetImage(*denoiseImageHandle);
    engine->_atrousFilterConstants.texelSize = { 1.0f / denoiseImage.imageExtent.width, 1.0f / denoiseImage.imageExtent.height };

    AllocatedImage atrousImage = engine->CreateImage("GPU_atrous image", denoiseImage.imageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    engine->GetCurrentFrame().deletionQueue.PushFunction([=]() {
        engine->DestroyImage(atrousImage);
    });

    DescriptorWriter writer;
    writer.WriteImage(0, denoiseImage.imageView, engine->_defaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.UpdateSet(engine->_device, atrousFilterDescriptorSet);

    std::vector<VkDescriptorSet> descriptorSets =
    {
        atrousFilterDescriptorSet
    };

   
    for (int i = 0; i < engine->_atrousFilterNumSamples; i++)
    {
        GPUDebugScope scope(cmd, "Atrous Filter Pass");

        engine->_atrousFilterConstants.stepWidth = static_cast<float>(1 << i);

        vkutil::TransitionImage(cmd, atrousImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(atrousImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingInfo renderInfo = vkinit::rendering_info(engine->_drawExtent, &colorAttachment, nullptr, nullptr);
        
        vkCmdBeginRendering(cmd, &renderInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(AtrousFilterConstants), &engine->_atrousFilterConstants);

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

        vkutil::TransitionImage(cmd, atrousImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vkutil::TransitionImage(cmd, denoiseImage.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vkutil::CopyImageToImage(cmd, atrousImage.image, denoiseImage.image, { atrousImage.imageExtent.width, atrousImage.imageExtent.height }, { denoiseImage.imageExtent.width, denoiseImage.imageExtent.height });

        vkutil::TransitionImage(cmd, denoiseImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        ++engine->_perfStats.drawcallCount;
    }
}