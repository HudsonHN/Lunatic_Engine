#include "vk_gbufferpass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"
#include "vk_debug.h"

void GBufferPass::DataSetup(LunaticEngine* engine)
{
    indexBufferHandle = &engine->_indexBuffer;
    surfaceMetaInfoBufferHandle = &engine->_surfaceMetaInfoBuffer;
    opaqueDrawCommandBufferHandle = &engine->_opaqueDrawCommandBuffer;
    opaqueDrawCountBufferHandle = &engine->_opaqueDrawCountBuffer;
    imageMetaInfoBufferHandle = &engine->_imageMetaInfoBuffer;
    lightsBufferHandle = &engine->_lightsBuffer;
    allMaterialImagesHandle = &engine->_allMaterialImages;

    positionColorHandle = &engine->_positionColor;
    normalColorHandle = &engine->_normalColor;
    albedoColorHandle = &engine->_albedoSpecColor;
    metalRoughColorHandle = &engine->_metalRoughnessColor;
    depthImageHandle = &engine->_depthImage;
    velocityImageHandle = &engine->_velocityImage;

    colorImages.push_back(positionColorHandle);
    colorImages.push_back(normalColorHandle);
    colorImages.push_back(albedoColorHandle);
    colorImages.push_back(metalRoughColorHandle);
    colorImages.push_back(velocityImageHandle);
    depthImage = depthImageHandle;
}

void GBufferPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES);

        VkDescriptorBindingFlags bindingFlags[] = {
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
        };
        gBufferDescriptorSetLayout = builder.Build(engine->_device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, bindingFlags);
        deletionQueue.PushFunction([=]() {
            vkDestroyDescriptorSetLayout(engine->_device, gBufferDescriptorSetLayout, nullptr);
            });
        gBufferDescriptorSet = descriptorAllocator->Allocate(engine->_device, gBufferDescriptorSetLayout, MAX_TEXTURES);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        historySceneDataDescriptorSetLayout = builder.Build(engine->_device);

        deletionQueue.PushFunction([=]()
            {
                vkDestroyDescriptorSetLayout(engine->_device, historySceneDataDescriptorSetLayout, nullptr);
            });
        historySceneDataDescriptorSet = descriptorAllocator->Allocate(engine->_device, historySceneDataDescriptorSetLayout);
    }

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = gBufferDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        surfaceMetaInfoBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = gBufferDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        imageMetaInfoBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        MultiDescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = gBufferDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        allMaterialImagesHandle->bindingInfos.push_back(bindingInfo);
    }
}

void GBufferPass::PipelineSetup(LunaticEngine* engine)
{
    VkShaderModule fragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\gbuffer.frag.spv", engine->_device, &fragShader)) {
        fmt::println("Error when building the deferred fragment shader module");
    }

    VkShaderModule vertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\gbuffer.vert.spv", engine->_device, &vertexShader)) {
        fmt::println("Error when building the deferred shader module");
    }

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts =
    {
        gBufferDescriptorSetLayout,
        historySceneDataDescriptorSetLayout
    };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(VkDeviceAddress);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    layoutInfo.pSetLayouts = descriptorSetLayouts.data();
    layoutInfo.pPushConstantRanges = &pushConstantRange;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &pipelineLayout));

    deletionQueue.PushFunction([=]() {
        vkDestroyPipelineLayout(engine->_device, pipelineLayout, nullptr);
    });

    // build the stage-create-info for both vertex and fragment stages. This lets
    // the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(vertexShader, fragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER);
    pipelineBuilder.enable_stenciltest(true, VK_COMPARE_OP_ALWAYS);

    //render format
    std::vector<VkFormat> imageFormats;
    imageFormats.reserve(colorImages.size());

    for (uint32_t i = 0; i < static_cast<uint32_t>(colorImages.size()); ++i)
    {
        imageFormats.push_back(renderGraph->GetImage(*colorImages[i]).imageFormat);
    }

    //pipelineBuilder.set_color_attachment_format(*imageFormats);
    pipelineBuilder._renderInfo.colorAttachmentCount = static_cast<uint32_t>(colorImages.size());
    pipelineBuilder._renderInfo.pColorAttachmentFormats = imageFormats.data();

    pipelineBuilder.set_depth_format(renderGraph->GetImage(*depthImage).imageFormat);
    pipelineBuilder.set_stencil_format(renderGraph->GetImage(*depthImage).imageFormat);

    // use the triangle layout we created
    pipelineBuilder._pipelineLayout = pipelineLayout;

    VkPipelineColorBlendAttachmentState blendAttachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments =
    {
        blendAttachment, blendAttachment, blendAttachment, blendAttachment, blendAttachment
    };

    // finally build the pipeline
    pipeline = pipelineBuilder.build_pipeline(engine->_device, static_cast<uint32_t>(blendAttachments.size()), blendAttachments.data());

    deletionQueue.PushFunction([=]()
    {
        vkDestroyPipeline(engine->_device, pipeline, nullptr);
    });

    vkDestroyShaderModule(engine->_device, fragShader, nullptr);
    vkDestroyShaderModule(engine->_device, vertexShader, nullptr);
}

void GBufferPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    AllocatedImage& positionColor = renderGraph->GetImage(*positionColorHandle);
    AllocatedImage& normalColor = renderGraph->GetImage(*normalColorHandle);
    AllocatedImage& albedoColor = renderGraph->GetImage(*albedoColorHandle);
    AllocatedImage& metalRoughColor = renderGraph->GetImage(*metalRoughColorHandle);
    AllocatedImage& velocityImage = renderGraph->GetImage(*velocityImageHandle);

    VkClearValue clearValue{};
    clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };
    VkRenderingAttachmentInfo positionAttachment = vkinit::attachment_info(positionColor.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo normalAttachment = vkinit::attachment_info(normalColor.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo albedoSpecAttachment = vkinit::attachment_info(albedoColor.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo metalRoughAttachment = vkinit::attachment_info(metalRoughColor.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo velocityImageAttachment = vkinit::attachment_info(velocityImage.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    std::array<VkRenderingAttachmentInfo, 5> colorAttachments = {
        positionAttachment,
        normalAttachment,
        albedoSpecAttachment,
        metalRoughAttachment,
        velocityImageAttachment,
    };

    AllocatedImage& depthImage = renderGraph->GetImage(*depthImageHandle);

    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(engine->_drawExtent, colorAttachments.data(), &depthAttachment, &depthAttachment);
    renderInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());

    AllocatedBuffer prevSceneDataBuffer = engine->CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "gpu old gbuffer scene data buffer");
    AllocatedBuffer sceneDataBuffer = engine->CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "gpu gbuffer scene data buffer");

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
        writer.UpdateSet(engine->_device, historySceneDataDescriptorSet);
    }

    std::vector<VkDescriptorSet> descriptorSets =
    {
        gBufferDescriptorSet,
        historySceneDataDescriptorSet
    };

    const AllocatedBuffer& indexBuffer = renderGraph->GetBuffer(*indexBufferHandle);
    const AllocatedBuffer& drawCmdBuffer = renderGraph->GetBuffer(*opaqueDrawCommandBufferHandle);
    const AllocatedBuffer& drawCountBuffer = renderGraph->GetBuffer(*opaqueDrawCountBufferHandle);

    GPUDebugScope scope(cmd, "GBuffer Pass");

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &engine->_vertexBufferAddress);

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
    vkCmdDrawIndexedIndirectCount(cmd, drawCmdBuffer.buffer, 0, drawCountBuffer.buffer, 0, drawCmdBuffer.usedSize / sizeof(VkDrawIndexedIndirectCommand), sizeof(VkDrawIndexedIndirectCommand));
    vkCmdEndRendering(cmd);
    ++engine->_perfStats.drawcallCount;
}