#pragma once

#include "vk_renderpass.h"
#include "vk_types.h"

struct IndirectLightingRSMCompute : RenderPass
{
	virtual void DataSetup(LunaticEngine* engine);
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) override;
	virtual void PipelineSetup(LunaticEngine* engine);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) override;

	ResourceHandle* positionColorHandle = nullptr;
	ResourceHandle* normalColorHandle = nullptr;
	ResourceHandle* albedoColorHandle = nullptr;
	ResourceHandle* metalRoughColorHandle = nullptr;
	ResourceHandle* indirectLightColorHandle = nullptr;
	ResourceHandle* noiseAOTextureHandle = nullptr;
	ResourceHandle* indirLightViewProjBufferHandle = nullptr;
	ResourceHandle* inverseIndirLightViewProjBufferHandle = nullptr;

	MultiImageHandle* rsmImagesHandle = nullptr;

	VkDescriptorSet gBufferDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout gBufferDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet indirectLightingRSMDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout indirectLightingRSMDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet indirectLightingRSMComputeDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout indirectLightingRSMComputeDescriptorSetLayout = VK_NULL_HANDLE;
};