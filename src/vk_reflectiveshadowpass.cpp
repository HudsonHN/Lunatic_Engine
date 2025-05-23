#include "vk_reflectiveshadowpass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"
#include "vk_debug.h"

void ReflectiveShadowPass::DataSetup(LunaticEngine* engine)
{
    for (int i = 0; i < NUM_CASCADES * 3; i += 3)
    {
        cascadePositionHandles[i / 3] = &engine->_cascadePosition[i / 3];
        cascadeNormalHandles[i / 3] = &engine->_cascadeNormal[i / 3];
        cascadeFluxHandles[i / 3] = &engine->_cascadeFlux[i / 3];
        cascadeDepthHandles[i / 3] = &engine->_cascadeReflectDepth[i / 3];
    }

    colorImages.push_back(cascadePositionHandles[0]);
    colorImages.push_back(cascadeNormalHandles[0]);
    colorImages.push_back(cascadeFluxHandles[0]);
    depthImage = cascadePositionHandles[0];

    surfaceMetaInfoBufferHandle = &engine->_surfaceMetaInfoBuffer;
    vertexBufferHandle = &engine->_vertexBuffer;
    indexBufferHandle = &engine->_indexBuffer;
    imageMetaInfoBufferHandle = &engine->_imageMetaInfoBuffer;
    opaqueDrawCommandBufferHandle = &engine->_opaqueDrawCommandBuffer;
    opaqueDrawCountBufferHandle = &engine->_opaqueDrawCountBuffer;
    allMaterialImagesHandle = &engine->_allMaterialImages;
}

void ReflectiveShadowPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    builder.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES);

    VkDescriptorBindingFlags bindlessFlags[] = {
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
    };

    reflectiveShadowsDescriptorSetLayout = builder.Build(engine->_device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, bindlessFlags);
    deletionQueue.PushFunction([=]()
        {
            vkDestroyDescriptorSetLayout(engine->_device, reflectiveShadowsDescriptorSetLayout, nullptr);
        });
    reflectiveShadowsDescriptorSet = descriptorAllocator->Allocate(engine->_device, reflectiveShadowsDescriptorSetLayout, MAX_TEXTURES);

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = reflectiveShadowsDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        surfaceMetaInfoBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = reflectiveShadowsDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        vertexBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = reflectiveShadowsDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        imageMetaInfoBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        MultiDescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 3;
        bindingInfo.descriptorSet = reflectiveShadowsDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        allMaterialImagesHandle->bindingInfos.push_back(bindingInfo);
    }
}

void ReflectiveShadowPass::PipelineSetup(LunaticEngine* engine)
{
    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(glm::mat4);
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts =
    {
        reflectiveShadowsDescriptorSetLayout,
        sceneDataDescriptorSetLayout
    };

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.pSetLayouts = descriptorSetLayouts.data();
    layoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    layoutInfo.pPushConstantRanges = &pushConstant;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &pipelineLayout));

    VkShaderModule vertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\light_reflective_shadows.vert.spv", engine->_device, &vertexShader))
    {
        fmt::print("Error when building the reflect shadow vert shader, cannot load file.\n");
    }

    VkShaderModule fragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\light_reflective_shadows.frag.spv", engine->_device, &fragShader))
    {
        fmt::print("Error when building the reflect shadow frag shader, cannot load file.\n");
    }

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(vertexShader, fragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_depth_format(VK_FORMAT_D32_SFLOAT);
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER);

    pipelineBuilder._pipelineLayout = pipelineLayout;

    std::vector<VkFormat> colorAttachments;
    colorAttachments.reserve(colorImages.size());
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
    blendAttachments.reserve(colorImages.size());
    VkPipelineColorBlendAttachmentState blendAttachment = {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    for (uint32_t i = 0; i < static_cast<uint32_t>(colorImages.size()); ++i)
    {
        AllocatedImage& colorImage = renderGraph->GetImage(*colorImages[i]);
        colorAttachments.push_back(colorImage.imageFormat);
        blendAttachments.push_back(blendAttachment);
    }

    pipelineBuilder.set_multi_color_attachment_format(colorAttachments);

    pipeline = pipelineBuilder.build_pipeline(engine->_device, static_cast<uint32_t>(blendAttachments.size()), blendAttachments.data());

    vkDestroyShaderModule(engine->_device, vertexShader, nullptr);
    vkDestroyShaderModule(engine->_device, fragShader, nullptr);

    deletionQueue.PushFunction([=]()
    {
        vkDestroyPipeline(engine->_device, pipeline, nullptr);
        vkDestroyPipelineLayout(engine->_device, pipelineLayout, nullptr);
    });
}

void ReflectiveShadowPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    /*if (engine->_prevCascadeExtentIndex != engine->_cascadeExtentIndex)
    {
        for (int i = 0; i < NUM_CASCADES; i++)
        {
            renderGraph->DestroyImage(*cascadeDepthHandles[i]);
            renderGraph->DestroyImage(*cascadePositionHandles[i]);
            renderGraph->DestroyImage(*cascadeNormalHandles[i]);
            renderGraph->DestroyImage(*cascadeFluxHandles[i]);
        }
        engine->InitReflectiveShadowData();

        engine->_prevCascadeExtentIndex = engine->_cascadeExtentIndex;
    }*/

    VkClearValue clearValue = { 0.0f, 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < NUM_CASCADES; i++)
    {
        AllocatedImage& positionImage = renderGraph->GetImage(*cascadePositionHandles[i]);
        AllocatedImage& normalImage = renderGraph->GetImage(*cascadeNormalHandles[i]);
        AllocatedImage& fluxImage = renderGraph->GetImage(*cascadeFluxHandles[i]);
        AllocatedImage& depthImage = renderGraph->GetImage(*cascadeDepthHandles[i]);

        VkRenderingAttachmentInfo positionAttachment = vkinit::attachment_info(positionImage.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo normalAttachment = vkinit::attachment_info(normalImage.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo fluxAttachment = vkinit::attachment_info(fluxImage.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        std::array<VkRenderingAttachmentInfo, 3> colorAttachments =
        {
            positionAttachment,
            normalAttachment,
            fluxAttachment
        };

        VkExtent2D extents = engine->_cascadeExtents[engine->_cascadeExtentIndex];
        extents.width /= 4;
        extents.height /= 4;

        VkRenderingInfo renderInfo = vkinit::rendering_info(extents, colorAttachments.data(), &depthAttachment, nullptr);
        renderInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());

        std::array<VkDescriptorSet, 2> descriptorSets =
        {
            reflectiveShadowsDescriptorSet,
            sceneDataDescriptorSet
        };

        AllocatedBuffer& indexBuffer = renderGraph->GetBuffer(*indexBufferHandle);

        GPUDebugScope scope(cmd, "Directional Reflective Shadow Pass");

        vkCmdBeginRendering(cmd, &renderInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &engine->_indirLightViewProj[i]);

        // Set dynamic viewport and scissor
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(extents.width);
        viewport.height = static_cast<float>(extents.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent = extents;

        vkCmdSetScissor(cmd, 0, 1, &scissor);

        AllocatedBuffer& opaqueDrawCommandBuffer = renderGraph->GetBuffer(*opaqueDrawCommandBufferHandle);
        AllocatedBuffer& opaqueDrawCountBuffer = renderGraph->GetBuffer(*opaqueDrawCountBufferHandle);

        vkCmdDrawIndexedIndirectCount(cmd, opaqueDrawCommandBuffer.buffer, 0, opaqueDrawCountBuffer.buffer, 0, opaqueDrawCommandBuffer.usedSize / sizeof(VkDrawIndexedIndirectCommand), sizeof(VkDrawIndexedIndirectCommand));

        vkCmdEndRendering(cmd);
        ++engine->_perfStats.drawcallCount;
    }
}