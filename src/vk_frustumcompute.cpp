#include "vk_frustumcompute.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "vk_engine.h"

void FrustumCompute::DataSetup(LunaticEngine* engine)
{
    surfaceMetaInfoBufferHandle = &engine->_surfaceMetaInfoBuffer;
    opaqueDrawCommandBufferHandle = &engine->_opaqueDrawCommandBuffer;
    transparentDrawCommandBufferHandle = &engine->_transparentDrawCommandBuffer;
    gpuTriangleCountBufferHandle = &engine->_gpuTriangleCountBuffer;
    cpuTriangleCountBufferHandle = &engine->_cpuTriangleCountBuffer;
}

void FrustumCompute::DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
    DescriptorLayoutBuilder builder;
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.AddBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    frustumComputeDescriptorSetLayout = builder.Build(engine->_device);
    deletionQueue.PushFunction([=]() {
        vkDestroyDescriptorSetLayout(engine->_device, frustumComputeDescriptorSetLayout, nullptr);
        });

    frustumComputeDescriptorSet = descriptorAllocator->Allocate(engine->_device, frustumComputeDescriptorSetLayout);
    
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 0;
        bindingInfo.descriptorSet = frustumComputeDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        surfaceMetaInfoBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 1;
        bindingInfo.descriptorSet = frustumComputeDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        opaqueDrawCommandBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 2;
        bindingInfo.descriptorSet = frustumComputeDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        transparentDrawCommandBufferHandle->bindingInfos.push_back(bindingInfo);
    }
    {
        DescriptorBindingInfo bindingInfo{};
        bindingInfo.binding = 3;
        bindingInfo.descriptorSet = frustumComputeDescriptorSet;
        bindingInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingInfo.isImage = false;

        gpuTriangleCountBufferHandle->bindingInfos.push_back(bindingInfo);
    }
}

void FrustumCompute::PipelineSetup(LunaticEngine* engine)
{
    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(FrustumCullConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo createInfo = vkinit::pipeline_layout_create_info();
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &frustumComputeDescriptorSetLayout;
    createInfo.pPushConstantRanges = &pushConstant;
    createInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine->_device, &createInfo, nullptr, &pipelineLayout));

    deletionQueue.PushFunction([=]()
        {
            vkDestroyPipelineLayout(engine->_device, pipelineLayout, nullptr);
        });

    VkShaderModule cullShader;
    if (!vkutil::load_shader_module(SHADER_PATH"\\frustum_cull.comp.spv", engine->_device, &cullShader))
    {
        fmt::print("Error when building the frustum culling compute shader, cannot load file.\n");
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = cullShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = pipelineLayout;
    computePipelineCreateInfo.stage = stageInfo;

    VK_CHECK(vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline));

    vkDestroyShaderModule(engine->_device, cullShader, nullptr);

    deletionQueue.PushFunction([=]() {
        vkDestroyPipeline(engine->_device, pipeline, nullptr);
        });
}

void FrustumCompute::Execute(LunaticEngine* engine, VkCommandBuffer cmd)
{
    AllocatedBuffer& gpuTriangleCountBuffer = renderGraph->GetBuffer(*gpuTriangleCountBufferHandle);

    vkCmdFillBuffer(cmd, gpuTriangleCountBuffer.buffer, 0, sizeof(uint32_t), 0);

    VkBufferMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.buffer = gpuTriangleCountBuffer.buffer;
    barrier.offset = 0;
    barrier.size = gpuTriangleCountBuffer.usedSize;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.bufferMemoryBarrierCount = 1;
    dependencyInfo.pBufferMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    // Bind the background compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    // Bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &frustumComputeDescriptorSet, 0, nullptr);

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FrustumCullConstants), &engine->_frustumCullConstants);

    // Execute the compute pipeline dispatch. We are using x=16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, renderGraph->GetBuffer(*surfaceMetaInfoBufferHandle).usedSize / 16, 1, 1);
}