#include "vk_transparentpass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"
#include "vk_debug.h"

void TransparentPass::DataSetup(LunaticEngine* engine)
{
    indexBufferHandle = &engine->_indexBuffer;
    surfaceMetaInfoBufferHandle = &engine->_surfaceMetaInfoBuffer;
    transparentDrawCommandBufferHandle = &engine->_transparentDrawCommandBuffer;
    transparentDrawCountBufferHandle = &engine->_transparentDrawCountBuffer;
    cubeMapImageHandle = &engine->_cubeMap;
    dirLightViewProjBufferHandle = &engine->_dirLightViewProjBuffer;
    drawImageHandle = &engine->_drawImage;

    imageMetaInfoBufferHandle = &engine->_imageMetaInfoBuffer;
    lightsBufferHandle = &engine->_lightsBuffer;
    allMaterialImagesHandle = &engine->_allMaterialImages;
    depthImageHandle = &engine->_depthImage;
    shadowMapImagesHandle = &engine->_shadowMapImages;
}

void TransparentPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES);

        VkDescriptorBindingFlags bindlessFlags[] = {
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
        };

        transparentDescriptorSetLayout = builder.Build(engine->_device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, bindlessFlags);
        deletionQueue.PushFunction([=]() {
            vkDestroyDescriptorSetLayout(engine->_device, transparentDescriptorSetLayout, nullptr);
        });

        transparentDescriptorSet = descriptorAllocator->Allocate(engine->_device, transparentDescriptorSetLayout, MAX_TEXTURES);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, NUM_CASCADES);

        VkDescriptorBindingFlags bindingFlags[] =
        {
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
        };

        shadowsDescriptorSetLayout = builder.Build(engine->_device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, bindingFlags);
        deletionQueue.PushFunction([=]()
            {
                vkDestroyDescriptorSetLayout(engine->_device, shadowsDescriptorSetLayout, nullptr);
            });
        shadowsDescriptorSet = descriptorAllocator->Allocate(engine->_device, shadowsDescriptorSetLayout, NUM_CASCADES);
    }

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = transparentDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        surfaceMetaInfoBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = transparentDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        imageMetaInfoBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = transparentDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        lightsBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 3;
        bindingInfo.descriptorSet = transparentDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageSampler = engine->_defaultSamplerLinear;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.isImage = true;

        cubeMapImageHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        MultiDescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 4;
        bindingInfo.descriptorSet = transparentDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        allMaterialImagesHandle->bindingInfos.push_back(bindingInfo);
    }

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = shadowsDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindingInfo.isImage = false;

        dirLightViewProjBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        MultiDescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = shadowsDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        shadowMapImagesHandle->bindingInfos.push_back(bindingInfo);
    }
}

void TransparentPass::PipelineSetup(LunaticEngine* engine)
{
    VkShaderModule vertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\transparent.vert.spv", engine->_device, &vertexShader)) {
        fmt::println("Error when building the transparent vertex shader module");
    }

    VkShaderModule fragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\transparent.frag.spv", engine->_device, &fragShader)) {
        fmt::println("Error when building the transparent fragment shader module");
    }

    std::vector<VkDescriptorSetLayout> layouts =
    {
        transparentDescriptorSetLayout,
        shadowsDescriptorSetLayout,
        sceneDataDescriptorSetLayout
    };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(VkDeviceAddress);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    layoutInfo.pSetLayouts = layouts.data();
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
    pipelineBuilder.enable_blending_additive();
    pipelineBuilder.disable_stenciltest();
    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER);
    pipelineBuilder.set_shaders(vertexShader, fragShader);
    pipelineBuilder.set_color_attachment_format(renderGraph->GetImage(*drawImageHandle).imageFormat);
    pipelineBuilder.set_depth_format(renderGraph->GetImage(*depthImageHandle).imageFormat);
    pipelineBuilder.set_stencil_format(renderGraph->GetImage(*depthImageHandle).imageFormat);

    pipelineBuilder._pipelineLayout = pipelineLayout;   
    pipeline = pipelineBuilder.build_pipeline(engine->_device, 1);

    deletionQueue.PushFunction([=]()
    {
        vkDestroyPipeline(engine->_device, pipeline, nullptr);
    });

    vkDestroyShaderModule(engine->_device, fragShader, nullptr);
    vkDestroyShaderModule(engine->_device, vertexShader, nullptr);
}

void TransparentPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    AllocatedImage& drawImage = renderGraph->GetImage(*drawImageHandle);
    AllocatedImage& depthImage = renderGraph->GetImage(*depthImageHandle);

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, nullptr, false);

    VkRenderingInfo renderInfo = vkinit::rendering_info(engine->_drawExtent, &colorAttachment, &depthAttachment, &depthAttachment);

    std::vector<VkDescriptorSet> descriptorSets =
    {
        transparentDescriptorSet,
        shadowsDescriptorSet,
        sceneDataDescriptorSet
    };
    
    AllocatedBuffer& indexBuffer = renderGraph->GetBuffer(*indexBufferHandle);

    GPUDebugScope debugScope(cmd, "Transparent Pass");

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

    AllocatedBuffer& drawCmdBuffer = renderGraph->GetBuffer(*transparentDrawCommandBufferHandle);
    AllocatedBuffer& drawCountBuffer = renderGraph->GetBuffer(*transparentDrawCountBufferHandle);

    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdDrawIndexedIndirectCount(cmd, drawCmdBuffer.buffer, 0, drawCountBuffer.buffer, 0, drawCmdBuffer.usedSize / sizeof(VkDrawIndexedIndirectCommand), sizeof(VkDrawIndexedIndirectCommand));
    vkCmdEndRendering(cmd);
    ++engine->_perfStats.drawcallCount;
}