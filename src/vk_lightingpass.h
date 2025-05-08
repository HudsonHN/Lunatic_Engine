#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct LightingPass : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;
	
	ResourceHandle* drawImageHandle = nullptr;

	ResourceHandle* positionColorHandle = nullptr;
	ResourceHandle* normalColorHandle = nullptr;
	ResourceHandle* albedoColorHandle = nullptr;
	ResourceHandle* metalRoughColorHandle = nullptr;
	ResourceHandle* depthImageHandle = nullptr;
	ResourceHandle* lightsBufferHandle = nullptr;
	ResourceHandle* imageMetaInfoBufferHandle = nullptr;
	ResourceHandle* indirectLightColorHandle = nullptr;
	ResourceHandle* ssaoColorHandle = nullptr;
	ResourceHandle* cubeMapImageHandle = nullptr;

	ResourceHandle* dirLightViewProjBufferHandle = nullptr;

	MultiImageHandle* shadowMapImagesHandle = nullptr;

	VkDescriptorSet lightingDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout lightingDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet shadowsDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout shadowsDescriptorSetLayout = VK_NULL_HANDLE;
};