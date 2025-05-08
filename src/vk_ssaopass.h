#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct SSAOPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* ssaoImageHandle = nullptr;
	ResourceHandle* noiseImageHandle = nullptr;
	ResourceHandle* kernelBufferHandle = nullptr;

	ResourceHandle* positionColorHandle = nullptr;
	ResourceHandle* normalColorHandle = nullptr;

	VkDescriptorSet ssaoDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout ssaoDescriptorSetLayout = VK_NULL_HANDLE;
};