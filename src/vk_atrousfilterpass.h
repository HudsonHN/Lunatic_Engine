#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct AtrousFilterPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* denoiseImageHandle = nullptr;
	ResourceHandle* positionColorHandle = nullptr;
	ResourceHandle* normalColorHandle = nullptr;

	VkDescriptorSet atrousFilterDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout atrousFilterDescriptorSetLayout = VK_NULL_HANDLE;
};