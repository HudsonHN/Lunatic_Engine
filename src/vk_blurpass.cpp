#include "vk_blurpass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"
#include <random>

void BlurPass::DataSetup(LunaticEngine* engine)
{
    imageHandle = &engine->_ssaoColor;
}

void BlurPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    blurDescriptorSetLayout = builder.Build(engine->_device);
    deletionQueue.PushFunction([=]()
    {
        vkDestroyDescriptorSetLayout(engine->_device, blurDescriptorSetLayout, nullptr);
    });
    blurDescriptorSet = descriptorAllocator->Allocate(engine->_device, blurDescriptorSetLayout);
}

void BlurPass::PipelineSetup(LunaticEngine* engine)
{
    VkShaderModule horizFragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\blur_horizontal.frag.spv", engine->_device, &horizFragShader)) {
        fmt::println("Error when building the blur fragment shader module");
    }

    VkShaderModule horizVertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\blur_horizontal.vert.spv", engine->_device, &horizVertexShader)) {
        fmt::println("Error when building the blur shader module");
    }

    VkShaderModule vertFragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\blur_vertical.frag.spv", engine->_device, &vertFragShader)) {
        fmt::println("Error when building the blur fragment shader module");
    }

    VkShaderModule vertVertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\blur_vertical.vert.spv", engine->_device, &vertVertexShader)) {
        fmt::println("Error when building the blur shader module");
    }

    VkPushConstantRange range{};
    range.offset = 0;
    range.size = sizeof(glm::vec3);
    range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo blurLayoutInfo = vkinit::pipeline_layout_create_info();
    blurLayoutInfo.setLayoutCount = 1;
    blurLayoutInfo.pSetLayouts = &blurDescriptorSetLayout;
    blurLayoutInfo.pushConstantRangeCount = 1;
    blurLayoutInfo.pPushConstantRanges = &range;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &blurLayoutInfo, nullptr, &pipelineLayout));

    deletionQueue.PushFunction([=]() {
        vkDestroyPipelineLayout(engine->_device, pipelineLayout, nullptr);
        });

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(horizVertexShader, horizFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(VK_FORMAT_R8G8B8A8_UNORM);
    pipelineBuilder._pipelineLayout = pipelineLayout;

    horizontalBlurPipeline = pipelineBuilder.build_pipeline(engine->_device, 1);

    pipelineBuilder.set_shaders(vertVertexShader, vertFragShader);
    verticalBlurPipeline = pipelineBuilder.build_pipeline(engine->_device, 1);

    deletionQueue.PushFunction([=]()
    {
        vkDestroyPipeline(engine->_device, horizontalBlurPipeline, nullptr);
        vkDestroyPipeline(engine->_device, verticalBlurPipeline, nullptr);
    });

    vkDestroyShaderModule(engine->_device, horizFragShader, nullptr);
    vkDestroyShaderModule(engine->_device, horizVertexShader, nullptr);
    vkDestroyShaderModule(engine->_device, vertFragShader, nullptr);
    vkDestroyShaderModule(engine->_device, vertVertexShader, nullptr);
}

void BlurPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    AllocatedImage& image = renderGraph->GetImage(*imageHandle); 

    AllocatedImage blurredImage = engine->CreateImage("GPU_blurred image", image.imageExtent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    engine->GetCurrentFrame().deletionQueue.PushFunction([=]() 
    {
        engine->DestroyImage(blurredImage);
    });

    vkutil::TransitionImage(cmd, blurredImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkClearValue clearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(blurredImage.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(engine->_drawExtent, &colorAttachment, nullptr, nullptr);

    DescriptorWriter writer;
    writer.WriteImage(0, image.imageView, engine->_defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.UpdateSet(engine->_device, blurDescriptorSet);

    std::vector<VkDescriptorSet> descriptorSets =
    {
        blurDescriptorSet
    };

    glm::vec3 resBlur = { engine->_drawExtent.width, engine->_ssaoBlurAmount, engine->_renderScale };

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, horizontalBlurPipeline);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec3), &resBlur);
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

    ////////////////////////////////////////////////////////////////////

    VkImageMemoryBarrier2 blurBarrier;

    blurBarrier = vkinit::pipeline_image_memory_barrier(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, blurredImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &blurBarrier;

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    vkutil::TransitionImage(cmd, blurredImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::TransitionImage(cmd, image.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkutil::CopyImageToImage(cmd, blurredImage.image, image.image, { image.imageExtent.width, image.imageExtent.height }, { image.imageExtent.width, image.imageExtent.height });

    vkutil::TransitionImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkutil::TransitionImage(cmd, blurredImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    ////////////////////////////////////////////////////////////////////

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, verticalBlurPipeline);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec3), &resBlur);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);

    ++engine->_perfStats.drawcallCount;

    vkutil::TransitionImage(cmd, blurredImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::TransitionImage(cmd, image.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkutil::CopyImageToImage(cmd, blurredImage.image, image.image, { image.imageExtent.width, image.imageExtent.height }, { image.imageExtent.width, image.imageExtent.height });

    vkutil::TransitionImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}