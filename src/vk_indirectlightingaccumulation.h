#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct IndirectLightingAccumulationCompute : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* positionColorHandle = nullptr;
	ResourceHandle* prevVelocityImageHandle = nullptr;
	ResourceHandle* currVelocityImageHandle = nullptr;
	ResourceHandle* indirectLightColorHandle = nullptr;
	ResourceHandle* prevIndirectLightColorHandle = nullptr;
	ResourceHandle* historyIndirectLightColorHandle = nullptr;

	VkDescriptorSet gBufferDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout gBufferDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet indirectLightingAccumulationComputeDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout indirectLightingAccumulationComputeDescriptorSetLayout = VK_NULL_HANDLE;
};