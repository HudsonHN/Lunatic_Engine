#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct GBufferPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* positionColorHandle = nullptr;
	ResourceHandle* normalColorHandle = nullptr;
	ResourceHandle* albedoColorHandle = nullptr;
	ResourceHandle* metalRoughColorHandle = nullptr;
	ResourceHandle* depthImageHandle = nullptr;
	ResourceHandle* velocityImageHandle = nullptr;
	ResourceHandle* vertexBufferHandle = nullptr;
	ResourceHandle* indexBufferHandle = nullptr;
	ResourceHandle* surfaceMetaInfoBufferHandle = nullptr;
	ResourceHandle* opaqueDrawCommandBufferHandle = nullptr;
	ResourceHandle* opaqueDrawCountBufferHandle = nullptr;
	ResourceHandle* imageMetaInfoBufferHandle = nullptr;
	ResourceHandle* lightsBufferHandle = nullptr;

	MultiImageHandle* allMaterialImagesHandle;

	VkDescriptorSet gBufferDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout gBufferDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet historySceneDataDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout historySceneDataDescriptorSetLayout = VK_NULL_HANDLE;
};