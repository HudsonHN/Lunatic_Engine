#include "vk_indirectlightingrsmcompute.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"

void IndirectLightingRSMCompute::DataSetup(LunaticEngine* engine)
{
    positionColorHandle = &engine->_positionColor;
    normalColorHandle = &engine->_normalColor;
    albedoColorHandle = &engine->_albedoSpecColor;
    metalRoughColorHandle = &engine->_metalRoughnessColor;

    noiseAOTextureHandle = &engine->_noiseAOTexture;
    indirLightViewProjBufferHandle = &engine->_indirLightViewProjBuffer;
    inverseIndirLightViewProjBufferHandle = &engine->_inverseIndirLightViewProjBuffer;
    rsmImagesHandle = &engine->_reflectiveShadowMapImages;
    
    indirectLightColorHandle = &engine->_indirectLightColor;
}

void IndirectLightingRSMCompute::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        gBufferDescriptorSetLayout = builder.Build(engine->_device);
        deletionQueue.PushFunction([=]() {
            vkDestroyDescriptorSetLayout(engine->_device, gBufferDescriptorSetLayout, nullptr);
            });
        gBufferDescriptorSet = descriptorAllocator->Allocate(engine->_device, gBufferDescriptorSetLayout);
    }

    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
        builder.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
        builder.AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
        builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, NUM_CASCADES * 3);

        VkDescriptorBindingFlags bindlessFlags[] = {
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
        };

        indirectLightingRSMDescriptorSetLayout = builder.Build(engine->_device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, bindlessFlags);
        deletionQueue.PushFunction([=]()
            {
                vkDestroyDescriptorSetLayout(engine->_device, indirectLightingRSMDescriptorSetLayout, nullptr);
            });
        indirectLightingRSMDescriptorSet = descriptorAllocator->Allocate(engine->_device, indirectLightingRSMDescriptorSetLayout, NUM_CASCADES * 3);
    }

    {
        DescriptorLayoutBuilder builder;
        builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);

        indirectLightingRSMComputeDescriptorSetLayout = builder.Build(engine->_device);
        deletionQueue.PushFunction([=]()
            {
                vkDestroyDescriptorSetLayout(engine->_device, indirectLightingRSMComputeDescriptorSetLayout, nullptr);
            });
        indirectLightingRSMComputeDescriptorSet = descriptorAllocator->Allocate(engine->_device, indirectLightingRSMComputeDescriptorSetLayout);
    }

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = gBufferDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        positionColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = gBufferDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        normalColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = gBufferDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        albedoColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 3;
        bindingInfo.descriptorSet = gBufferDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        metalRoughColorHandle->bindingInfos.push_back(bindingInfo);
    }

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = indirectLightingRSMDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerRepeat;
        bindingInfo.isImage = true;

        noiseAOTextureHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = indirectLightingRSMDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindingInfo.isImage = false;

        indirLightViewProjBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = indirectLightingRSMDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindingInfo.isImage = false;

        inverseIndirLightViewProjBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        MultiDescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 3;
        bindingInfo.descriptorSet = indirectLightingRSMDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        rsmImagesHandle->bindingInfos.push_back(bindingInfo);
    }

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = indirectLightingRSMComputeDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        indirectLightColorHandle->bindingInfos.push_back(bindingInfo);
    }
}

void IndirectLightingRSMCompute::PipelineSetup(LunaticEngine* engine)
{
    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(DeferredLightingConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    std::vector<VkDescriptorSetLayout> layouts =
    {
        gBufferDescriptorSetLayout,
        sceneDataDescriptorSetLayout,
        indirectLightingRSMDescriptorSetLayout,
        indirectLightingRSMComputeDescriptorSetLayout
    };

    VkPipelineLayoutCreateInfo createInfo = vkinit::pipeline_layout_create_info();
    createInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    createInfo.pSetLayouts = layouts.data();
    createInfo.pPushConstantRanges = &pushConstant;
    createInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &createInfo, nullptr, &pipelineLayout));

    deletionQueue.PushFunction([=]()
    {
        vkDestroyPipelineLayout(engine->_device, pipelineLayout, nullptr);
    });

    VkShaderModule compShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\indirect_lighting_rsm.comp.spv", engine->_device, &compShader))
    {
        fmt::print("Error when building the frustum indirect rsm compute shader, cannot load file.\n");
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = compShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = pipelineLayout;
    computePipelineCreateInfo.stage = stageInfo;

    VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline));

    vkDestroyShaderModule(engine->_device, compShader, nullptr);

    deletionQueue.PushFunction([=]() {
        vkDestroyPipeline(engine->_device, pipeline, nullptr);
    });
}

void IndirectLightingRSMCompute::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    std::vector<VkDescriptorSet> descriptorSets =
    {
        gBufferDescriptorSet,
        sceneDataDescriptorSet,
        indirectLightingRSMDescriptorSet,
        indirectLightingRSMComputeDescriptorSet
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DeferredLightingConstants), &engine->_lightingConstants);

    vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(engine->_drawExtent.width / 8.0)), static_cast<uint32_t>(std::ceil(engine->_drawExtent.height / 8.0)), 1);
}