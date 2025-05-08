#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct SkyboxPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine) override;
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* drawImageHandle = nullptr;
	ResourceHandle* depthImageHandle = nullptr;
	ResourceHandle* cubeMapHandle = nullptr;

	VkDescriptorSet skyboxDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout skyboxDescriptorSetLayout = VK_NULL_HANDLE;
};