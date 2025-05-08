#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct TAAPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* prevFrameImageHandle = nullptr;
	ResourceHandle* drawImageHandle = nullptr;
	ResourceHandle* positionColorHandle = nullptr;
	ResourceHandle* historyImageHandle = nullptr;
	ResourceHandle* velocityImageHandle = nullptr;
	ResourceHandle* prevVelocityImageHandle = nullptr;

	std::vector<glm::vec2> haltonJitterOffsets;

	VkDescriptorSet temporalAADescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout temporalAADescriptorSetLayout = VK_NULL_HANDLE;

	VkDescriptorSet temporalAASceneDataDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout temporalAASceneDataDescriptorSetLayout = VK_NULL_HANDLE;
};