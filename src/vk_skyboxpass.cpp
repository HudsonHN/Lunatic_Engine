#include "vk_skyboxpass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"

void SkyboxPass::DataSetup(LunaticEngine* engine)
{
    cubeMapHandle = &engine->_cubeMap;
    depthImageHandle = &engine->_depthImage;
    drawImageHandle = &engine->_drawImage;
}

void SkyboxPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    skyboxDescriptorSetLayout = builder.Build(engine->_device);
    deletionQueue.PushFunction([=]() {
        vkDestroyDescriptorSetLayout(engine->_device, skyboxDescriptorSetLayout, nullptr);
    });

    skyboxDescriptorSet = descriptorAllocator->Allocate(engine->_device, skyboxDescriptorSetLayout);   

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = skyboxDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerLinear;
        bindingInfo.isImage = true;

        cubeMapHandle->bindingInfos.push_back(bindingInfo);
    }
}

void SkyboxPass::PipelineSetup(LunaticEngine* engine)
{
    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(SkyboxConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.pSetLayouts = &skyboxDescriptorSetLayout;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &pipelineLayout));

    VkShaderModule vertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\skybox.vert.spv", engine->_device, &vertexShader))
    {
        fmt::print("Error when building the skybox vert shader, cannot load file.\n");
    }

    VkShaderModule fragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\skybox.frag.spv", engine->_device, &fragShader))
    {
        fmt::print("Error when building the skybox frag shader, cannot load file.\n");
    }

    AllocatedImage& drawImage = renderGraph->GetImage(*drawImageHandle);
    AllocatedImage& depthImage = renderGraph->GetImage(*depthImageHandle);

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(vertexShader, fragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(drawImage.imageFormat);
    pipelineBuilder.set_depth_format(depthImage.imageFormat);
    pipelineBuilder.set_stencil_format(depthImage.imageFormat);
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

void SkyboxPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    AllocatedImage& drawImage = renderGraph->GetImage(*drawImageHandle);
    AllocatedImage& depthImage = renderGraph->GetImage(*depthImageHandle);

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, nullptr, false);

    VkRenderingInfo renderInfo = vkinit::rendering_info(engine->_drawExtent, &colorAttachment, &depthAttachment, &depthAttachment);

    SkyboxConstants constants
    {
        .viewProj = engine->_sceneData.viewProj,
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &skyboxDescriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SkyboxConstants), &constants);

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