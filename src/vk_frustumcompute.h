#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct FrustumCompute : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* surfaceMetaInfoBufferHandle = nullptr;
	ResourceHandle* opaqueDrawCommandBufferHandle = nullptr;
	ResourceHandle* transparentDrawCommandBufferHandle = nullptr;
	ResourceHandle* gpuTriangleCountBufferHandle = nullptr;
	ResourceHandle* cpuTriangleCountBufferHandle = nullptr;

	VkDescriptorSet frustumComputeDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout frustumComputeDescriptorSetLayout = VK_NULL_HANDLE;
};