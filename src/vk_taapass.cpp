#include "vk_taapass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"
#include "vk_debug.h"

void TAAPass::DataSetup(LunaticEngine* engine)
{
    historyImageHandle = &engine->_historyImage;
    velocityImageHandle = &engine->_velocityImage;
    prevVelocityImageHandle = &engine->_prevVelocityImage;
    prevFrameImageHandle = &engine->_prevFrameImage;
    drawImageHandle = &engine->_drawImage;
    positionColorHandle = &engine->_positionColor;

    colorImages.push_back(historyImageHandle);
}

void TAAPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        temporalAADescriptorSetLayout = builder.Build(engine->_device);
        deletionQueue.PushFunction([=]()
        {
            vkDestroyDescriptorSetLayout(engine->_device, temporalAADescriptorSetLayout, nullptr);
        });
        temporalAADescriptorSet = descriptorAllocator->Allocate(engine->_device, temporalAADescriptorSetLayout);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        temporalAASceneDataDescriptorSetLayout = builder.Build(engine->_device);

        deletionQueue.PushFunction([=]()
            {
                vkDestroyDescriptorSetLayout(engine->_device, temporalAASceneDataDescriptorSetLayout, nullptr);
            });
        temporalAASceneDataDescriptorSet = descriptorAllocator->Allocate(engine->_device, temporalAASceneDataDescriptorSetLayout);
    }

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = temporalAADescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerLinear;
        bindingInfo.isImage = true;

        prevFrameImageHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = temporalAADescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        drawImageHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = temporalAADescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        positionColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 3;
        bindingInfo.descriptorSet = temporalAADescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        prevVelocityImageHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 4;
        bindingInfo.descriptorSet = temporalAADescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        velocityImageHandle->bindingInfos.push_back(bindingInfo);
    }
}

void TAAPass::PipelineSetup(LunaticEngine* engine)
{
    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts =
    {
        temporalAADescriptorSetLayout,
        temporalAASceneDataDescriptorSetLayout
    };

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.pSetLayouts = descriptorSetLayouts.data();
    layoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &pipelineLayout));

    VkShaderModule vertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\temporal_anti_aliasing_resolve.vert.spv", engine->_device, &vertexShader))
    {
        fmt::print("Error when building the TAA vert shader, cannot load file.\n");
    }

    VkShaderModule fragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\temporal_anti_aliasing_resolve.frag.spv", engine->_device, &fragShader))
    {
        fmt::print("Error when building the TAA frag shader, cannot load file.\n");
    }

    VkPipelineColorBlendAttachmentState blendAttachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    std::vector<VkFormat> formats;
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
    for (uint32_t i = 0; i < colorImages.size(); ++i)
    {
        const AllocatedImage& colorImage = renderGraph->GetImage(*colorImages[i]);
        formats.push_back(colorImage.imageFormat);
        blendAttachments.push_back(blendAttachment);
    }

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(vertexShader, fragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_multi_color_attachment_format(formats);
    pipelineBuilder.disable_depthtest();

    pipelineBuilder._pipelineLayout = pipelineLayout;

    pipeline = pipelineBuilder.build_pipeline(engine->_device, static_cast<uint32_t>(blendAttachments.size()), blendAttachments.data());

    vkDestroyShaderModule(engine->_device, vertexShader, nullptr);
    vkDestroyShaderModule(engine->_device, fragShader, nullptr);

    deletionQueue.PushFunction([=]()
    {
        vkDestroyPipeline(engine->_device, pipeline, nullptr);
        vkDestroyPipelineLayout(engine->_device, pipelineLayout, nullptr);
    });
}

void TAAPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    AllocatedImage& historyImage = renderGraph->GetImage(*historyImageHandle);
    AllocatedImage& velocityImage = renderGraph->GetImage(*velocityImageHandle);

    VkClearValue clearVelocityValue = { 0.0f, 0.0f };
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(historyImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    std::array<VkRenderingAttachmentInfo, 1> colorAttachments =
    {
        colorAttachment
    };

    VkRenderingInfo renderInfo = vkinit::rendering_info(engine->_drawExtent, colorAttachments.data(), nullptr, nullptr);
    renderInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());

    AllocatedBuffer sceneDataBuffer = engine->CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "gpu taa scene data buffer");
    AllocatedBuffer prevSceneDataBuffer = engine->CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "gpu old taa scene data buffer");

    // Add it to the deletion queue of this frame so it gets deleted once its been used
    engine->GetCurrentFrame().deletionQueue.PushFunction([=, this]() {
        engine->DestroyBuffer(sceneDataBuffer);
        engine->DestroyBuffer(prevSceneDataBuffer);
    });

    void* prevGpuSceneData = nullptr;
    vmaMapMemory(engine->_allocator, prevSceneDataBuffer.allocation, &prevGpuSceneData);
    memcpy(prevGpuSceneData, &engine->_prevSceneData, sizeof(GPUSceneData));
    vmaUnmapMemory(engine->_allocator, prevSceneDataBuffer.allocation);

    void* gpuSceneData = nullptr;
    vmaMapMemory(engine->_allocator, sceneDataBuffer.allocation, &gpuSceneData);
    memcpy(gpuSceneData, &engine->_sceneData, sizeof(GPUSceneData));
    vmaUnmapMemory(engine->_allocator, sceneDataBuffer.allocation);

    {
        DescriptorWriter writer;
        writer.WriteBuffer(0, prevSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.WriteBuffer(1, sceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.UpdateSet(engine->_device, temporalAASceneDataDescriptorSet);
    }

    std::array<VkDescriptorSet, 2> descriptorSets =
    {
        temporalAADescriptorSet,
        temporalAASceneDataDescriptorSet
    };

    GPUDebugScope scope(cmd, "TAA Pass");

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
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