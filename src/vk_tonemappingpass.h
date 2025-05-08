#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct TonemappingPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine) override;
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* inputImageHandle = nullptr;
	ResourceHandle* outputImageHandle = nullptr;

	VkDescriptorSet tonemapDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout tonemapDescriptorSetLayout = VK_NULL_HANDLE;
};