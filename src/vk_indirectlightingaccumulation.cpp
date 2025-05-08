#include "vk_indirectlightingaccumulation.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"

void IndirectLightingAccumulationCompute::DataSetup(LunaticEngine* engine)
{
    prevIndirectLightColorHandle = &engine->_prevIndirectLightColor;
    indirectLightColorHandle = &engine->_indirectLightColor;
    positionColorHandle = &engine->_positionColor;
    prevVelocityImageHandle = &engine->_prevVelocityImage;
    historyIndirectLightColorHandle = &engine->_historyIndirectLightColor;
}

void IndirectLightingAccumulationCompute::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.AddBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.AddBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.AddBinding(6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    indirectLightingAccumulationComputeDescriptorSetLayout = builder.Build(engine->_device);
    deletionQueue.PushFunction([=]()
    {
        vkDestroyDescriptorSetLayout(engine->_device, indirectLightingAccumulationComputeDescriptorSetLayout, nullptr);
    });
    indirectLightingAccumulationComputeDescriptorSet = descriptorAllocator->Allocate(engine->_device, indirectLightingAccumulationComputeDescriptorSetLayout);

    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = indirectLightingAccumulationComputeDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        prevIndirectLightColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = indirectLightingAccumulationComputeDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        indirectLightColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = indirectLightingAccumulationComputeDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        positionColorHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 3;
        bindingInfo.descriptorSet = indirectLightingAccumulationComputeDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        prevVelocityImageHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 4;
        bindingInfo.descriptorSet = indirectLightingAccumulationComputeDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindingInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        bindingInfo.imageSampler = engine->_defaultSamplerNearest;
        bindingInfo.isImage = true;

        historyIndirectLightColorHandle->bindingInfos.push_back(bindingInfo);
    }
}

void IndirectLightingAccumulationCompute::PipelineSetup(LunaticEngine* engine)
{
    std::vector<VkDescriptorSetLayout> layouts =
    {
        indirectLightingAccumulationComputeDescriptorSetLayout
    };

    VkPipelineLayoutCreateInfo createInfo = vkinit::pipeline_layout_create_info();
    createInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    createInfo.pSetLayouts = layouts.data();

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &createInfo, nullptr, &pipelineLayout));

    deletionQueue.PushFunction([=]()
    {
        vkDestroyPipelineLayout(engine->_device, pipelineLayout, nullptr);
    });

    VkShaderModule compShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\indirect_lighting_temporal_resolve.comp.spv", engine->_device, &compShader))
    {
        fmt::print("Error when building the indirect rsm TAA compute shader, cannot load file.\n");
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

void IndirectLightingAccumulationCompute::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
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
        writer.WriteBuffer(5, prevSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.WriteBuffer(6, sceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.UpdateSet(engine->_device, indirectLightingAccumulationComputeDescriptorSet);
    }

    std::vector<VkDescriptorSet> descriptorSets =
    {
        indirectLightingAccumulationComputeDescriptorSet
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

    vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(engine->_drawExtent.width / 8.0)), static_cast<uint32_t>(std::ceil(engine->_drawExtent.height / 8.0)), 1);
}