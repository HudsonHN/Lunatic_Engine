#include "vk_tonemappingpass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"

void TonemappingPass::DataSetup(LunaticEngine* engine)
{
    outputImageHandle = &engine->_intermediateImage;
    inputImageHandle = &engine->_drawImage;
}

void TonemappingPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{  
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    tonemapDescriptorSetLayout = builder.Build(engine->_device);
    deletionQueue.PushFunction([=]() {
        vkDestroyDescriptorSetLayout(engine->_device, tonemapDescriptorSetLayout, nullptr);
    });

    tonemapDescriptorSet = descriptorAllocator->Allocate(engine->_device, tonemapDescriptorSetLayout);

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = tonemapDescriptorSet;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerLinear;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.isImage = true;

        inputImageHandle->bindingInfos.push_back(bindingInfo);
    }
}

void TonemappingPass::PipelineSetup(LunaticEngine* engine)
{
    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(glm::vec2);
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.pSetLayouts = &tonemapDescriptorSetLayout;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &pipelineLayout));

    VkShaderModule vertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\tonemap.vert.spv", engine->_device, &vertexShader))
    {
        fmt::print("Error when building the tonemap vert shader, cannot load file.\n");
    }

    VkShaderModule fragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\tonemap.frag.spv", engine->_device, &fragShader))
    {
        fmt::print("Error when building the tonemap frag shader, cannot load file.\n");
    }

    VkFormat colorAttachmentFormat = renderGraph->GetImage(*outputImageHandle).imageFormat;

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(vertexShader, fragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(colorAttachmentFormat);

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

void TonemappingPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    AllocatedImage& outputImage = renderGraph->GetImage(*outputImageHandle);

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(outputImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(engine->_drawExtent, &colorAttachment, nullptr, nullptr);

    glm::vec2 tonemapData = { engine->_cameraExposure, engine->_renderScale };

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &tonemapDescriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec2), &tonemapData);

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