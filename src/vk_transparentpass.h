#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct TransparentPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* drawImageHandle = nullptr;
	ResourceHandle* depthImageHandle = nullptr;
	ResourceHandle* indexBufferHandle = nullptr;
	ResourceHandle* surfaceMetaInfoBufferHandle = nullptr;
	ResourceHandle* transparentDrawCommandBufferHandle = nullptr;
	ResourceHandle* transparentDrawCountBufferHandle = nullptr;
	ResourceHandle* imageMetaInfoBufferHandle = nullptr;
	ResourceHandle* cubeMapImageHandle = nullptr;

	ResourceHandle* dirLightViewProjBufferHandle = nullptr;
	ResourceHandle* lightsBufferHandle = nullptr;

	MultiImageHandle* shadowMapImagesHandle = nullptr;
	MultiImageHandle* allMaterialImagesHandle = nullptr;

	VkDescriptorSet transparentDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout transparentDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet shadowsDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout shadowsDescriptorSetLayout = VK_NULL_HANDLE;
};