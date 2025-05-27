#include "vk_cascadeshadowpass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"
#include "vk_debug.h"

void CascadeShadowPass::DataSetup(LunaticEngine* engine)
{
    surfaceMetaInfoBufferHandle = &engine->_surfaceMetaInfoBuffer;
    indexBufferHandle = &engine->_indexBuffer;
    opaqueDrawCommandBufferHandle = &engine->_opaqueDrawCommandBuffer;
    opaqueDrawCountBufferHandle = &engine->_opaqueDrawCountBuffer;
    for (uint32_t i = 0; i < NUM_CASCADES; ++i)
    {
        cascadeDepthHandles[i] = &engine->_cascadeDepth[i];
    }
}

void CascadeShadowPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    shadowMapDescriptorSetLayout = builder.Build(engine->_device);
    deletionQueue.PushFunction([=]() {
        vkDestroyDescriptorSetLayout(engine->_device, shadowMapDescriptorSetLayout, nullptr);
        });

    shadowMapDescriptorSet = descriptorAllocator->Allocate(engine->_device, shadowMapDescriptorSetLayout);

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = shadowMapDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        surfaceMetaInfoBufferHandle->bindingInfos.push_back(bindingInfo);
    }
}

void CascadeShadowPass::PipelineSetup(LunaticEngine* engine)
{
    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(CascadeConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.pSetLayouts = &shadowMapDescriptorSetLayout;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &pipelineLayout));

    VkShaderModule vertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\light_depth.vert.spv", engine->_device, &vertexShader))
    {
        fmt::print("Error when building the light depth vert shader, cannot load file.\n");
    }

    VkShaderModule fragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\light_depth.frag.spv", engine->_device, &fragShader))
    {
        fmt::print("Error when building the light depth frag shader, cannot load file.\n");
    }

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(vertexShader, fragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_depth_format(VK_FORMAT_D32_SFLOAT);
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER);

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

void CascadeShadowPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    /*if (engine->_prevCascadeExtentIndex != engine->_cascadeExtentIndex)
    {
        for (int i = 0; i < NUM_CASCADES; i++)
        {
            renderGraph->DestroyImage(*cascadeDepthHandles[i]);
        }
        engine->InitShadowMapData();

        engine->_prevCascadeExtentIndex = engine->_cascadeExtentIndex;
    }*/

    for (int i = 0; i < NUM_CASCADES; i++)
    {
        AllocatedImage& depthImage = renderGraph->GetImage(*cascadeDepthHandles[i]);

        VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        VkRenderingInfo renderInfo = vkinit::rendering_info(engine->_cascadeExtents[engine->_cascadeExtentIndex], nullptr, &depthAttachment, nullptr);
        renderInfo.colorAttachmentCount = 0;

        AllocatedBuffer& indexBuffer = renderGraph->GetBuffer(*indexBufferHandle);

        GPUDebugScope scope(cmd, "Directional Shadow Pass");

        CascadeConstants constants = 
        {
            .viewProj = engine->_dirLightViewProj[i],
            .vertexBuffer = engine->_vertexBufferAddress
        };

        vkCmdBeginRendering(cmd, &renderInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &shadowMapDescriptorSet, 0, nullptr);
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CascadeConstants), &constants);

        // Set dynamic viewport and scissor
        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(engine->_cascadeExtents[engine->_cascadeExtentIndex].width);
        viewport.height = static_cast<float>(engine->_cascadeExtents[engine->_cascadeExtentIndex].height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = engine->_cascadeExtents[engine->_cascadeExtentIndex].width;
        scissor.extent.height = engine->_cascadeExtents[engine->_cascadeExtentIndex].height;

        vkCmdSetScissor(cmd, 0, 1, &scissor);

        AllocatedBuffer& opaqueDrawCommandBuffer = renderGraph->GetBuffer(*opaqueDrawCommandBufferHandle);
        AllocatedBuffer& opaqueDrawCountBuffer = renderGraph->GetBuffer(*opaqueDrawCountBufferHandle);

        vkCmdDrawIndexedIndirectCount(cmd, opaqueDrawCommandBuffer.buffer, 0, opaqueDrawCountBuffer.buffer, 0, opaqueDrawCommandBuffer.usedSize / sizeof(VkDrawIndexedIndirectCommand), sizeof(VkDrawIndexedIndirectCommand));

        vkCmdEndRendering(cmd);
        ++engine->_perfStats.drawcallCount;
    }
}