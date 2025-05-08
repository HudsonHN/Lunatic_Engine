#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct ReflectiveShadowPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* cascadePositionHandles[NUM_CASCADES];
	ResourceHandle* cascadeNormalHandles[NUM_CASCADES];
	ResourceHandle* cascadeFluxHandles[NUM_CASCADES];
	ResourceHandle* cascadeDepthHandles[NUM_CASCADES];
	ResourceHandle* surfaceMetaInfoBufferHandle = nullptr;
	ResourceHandle* vertexBufferHandle = nullptr;
	ResourceHandle* indexBufferHandle = nullptr;
	ResourceHandle* imageMetaInfoBufferHandle = nullptr;
	ResourceHandle* opaqueDrawCommandBufferHandle = nullptr;
	ResourceHandle* opaqueDrawCountBufferHandle = nullptr;
	MultiImageHandle* allMaterialImagesHandle = nullptr;

	VkDescriptorSet reflectiveShadowsDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout reflectiveShadowsDescriptorSetLayout = VK_NULL_HANDLE;
};