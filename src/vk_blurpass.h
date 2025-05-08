#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct BlurPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* imageHandle = nullptr;

	VkDescriptorSet blurDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout blurDescriptorSetLayout = VK_NULL_HANDLE;

	VkPipeline horizontalBlurPipeline;
	VkPipeline verticalBlurPipeline;
};