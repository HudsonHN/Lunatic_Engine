#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct CascadeShadowPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* cascadeDepthHandles[NUM_CASCADES];
	ResourceHandle* surfaceMetaInfoBufferHandle = nullptr;
	ResourceHandle* vertexBufferHandle = nullptr;
	ResourceHandle* indexBufferHandle = nullptr;
	ResourceHandle* opaqueDrawCommandBufferHandle = nullptr;
	ResourceHandle* opaqueDrawCountBufferHandle = nullptr;

	VkDescriptorSet shadowMapDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout shadowMapDescriptorSetLayout = VK_NULL_HANDLE;
};