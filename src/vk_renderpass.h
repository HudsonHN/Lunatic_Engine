#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <vector>
#include "vk_types.h"
#include "vk_rendergraph.h"

class LunaticEngine;
struct DescriptorAllocatorGrowable;

struct RenderPass
{
	virtual ~RenderPass();
	virtual void DataSetup(LunaticEngine* engine) = 0;
	virtual void DescriptorSetup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator) = 0;
	virtual void PipelineSetup(LunaticEngine* engine) = 0;
	virtual void Setup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator);
	virtual void Execute(LunaticEngine* engine, VkCommandBuffer cmd) = 0;

	RenderGraph* renderGraph = nullptr;
	VkDescriptorSet sceneDataDescriptorSet;
	VkDescriptorSetLayout sceneDataDescriptorSetLayout;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	std::vector<ResourceHandle*> colorImages;
	ResourceHandle* depthImage = nullptr;
	DeletionQueue deletionQueue;
};