#include "vk_lightingpass.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"
#include "vk_debug.h"

void LightingPass::DataSetup(LunaticEngine* engine)
{
    drawImageHandle = &engine->_drawImage;
    positionColorHandle = &engine->_positionColor;
    normalColorHandle = &engine->_normalColor;
    albedoColorHandle = &engine->_albedoSpecColor;
    metalRoughColorHandle = &engine->_metalRoughnessColor;
    lightsBufferHandle = &engine->_lightsBuffer;
    ssaoColorHandle = &engine->_ssaoColor;
    cubeMapImageHandle = &engine->_cubeMap;
    indirectLightColorHandle = &engine->_indirectLightColor;
    dirLightViewProjBufferHandle = &engine->_dirLightViewProjBuffer;
    shadowMapImagesHandle = &engine->_shadowMapImages;
}

void LightingPass::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        builder.AddBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        lightingDescriptorSetLayout = builder.Build(engine->_device);
        deletionQueue.PushFunction([=]() {
            vkDestroyDescriptorSetLayout(engine->_device, lightingDescriptorSetLayout, nullptr);
            });
        lightingDescriptorSet = descriptorAllocator->Allocate(engine->_device, lightingDescriptorSetLayout);
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
        bindingInfo.descriptorSet = lightingDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        positionColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = lightingDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        normalColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = lightingDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerLinear;
        bindingInfo.isImage = true;

        albedoColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 3;
        bindingInfo.descriptorSet = lightingDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        metalRoughColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 4;
        bindingInfo.descriptorSet = lightingDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        lightsBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 5;
        bindingInfo.descriptorSet = lightingDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerLinear;
        bindingInfo.isImage = true;

        cubeMapImageHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 6;
        bindingInfo.descriptorSet = lightingDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerLinear;
        bindingInfo.isImage = true;

        ssaoColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 7;
        bindingInfo.descriptorSet = lightingDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerLinear;
        bindingInfo.isImage = true;

        indirectLightColorHandle->bindingInfos.push_back(bindingInfo);
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

void LightingPass::PipelineSetup(LunaticEngine* engine)
{
    VkShaderModule fragShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\lighting_deferred.frag.spv", engine->_device, &fragShader)) {
        fmt::println("Error when building the deferred fragment shader module");
    }

    VkShaderModule vertexShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\lighting_deferred.vert.spv", engine->_device, &vertexShader)) {
        fmt::println("Error when building the deferred shader module");
    }

    std::vector<VkDescriptorSetLayout> layouts =
    {
        lightingDescriptorSetLayout,
        sceneDataDescriptorSetLayout,
        shadowsDescriptorSetLayout
    };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.size = sizeof(DeferredLightingConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
    layoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    layoutInfo.pSetLayouts = layouts.data();
    layoutInfo.pPushConstantRanges = &pushConstantRange;
    layoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &layoutInfo, nullptr, &pipelineLayout));

    deletionQueue.PushFunction([=]() {
        vkDestroyPipelineLayout(engine->_device, pipelineLayout, nullptr);
        });

    AllocatedImage& drawImage = renderGraph->GetImage(*drawImageHandle);

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(vertexShader, fragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.set_color_attachment_format(drawImage.imageFormat);
    pipelineBuilder._pipelineLayout = pipelineLayout;

    pipeline = pipelineBuilder.build_pipeline(engine->_device, 1);

    deletionQueue.PushFunction([=]()
        {
            vkDestroyPipeline(engine->_device, pipeline, nullptr);
        });

    vkDestroyShaderModule(engine->_device, fragShader, nullptr);
    vkDestroyShaderModule(engine->_device, vertexShader, nullptr);
}

void LightingPass::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    AllocatedImage& drawImage = renderGraph->GetImage(*drawImageHandle);

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(engine->_drawExtent, &colorAttachment, nullptr, nullptr);

    std::vector<VkDescriptorSet> descriptorSets =
    {
        lightingDescriptorSet,
        sceneDataDescriptorSet,
        shadowsDescriptorSet
    };

    GPUDebugScope scope(cmd, "Opaque Lighting Pass");

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DeferredLightingConstants), &engine->_lightingConstants);

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