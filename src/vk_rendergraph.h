#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <set>
#include "vk_types.h"
#include "vk_descriptors.h"

class LunaticEngine;
struct RenderPass;
struct DescriptorAllocatorGrowable;

struct RenderGraph
{
public:
	LunaticEngine* engine = nullptr;
	std::vector<RenderPass*> renderPasses;

	VkDescriptorSet sceneDataDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout sceneDataDescriptorSetLayout = VK_NULL_HANDLE;

	DescriptorAllocatorGrowable* globalAllocator = nullptr;
	DescriptorAllocatorGrowable* bindlessAllocator = nullptr;

	AllocatedImage* drawImage = nullptr;
	AllocatedImage* intermediateImage = nullptr;
	AllocatedImage* depthImage = nullptr;

	struct FrustumCompute* frustumCompute = nullptr;
	struct GBufferPass* gBufferPass = nullptr;
	struct CascadeShadowPass* cascadeShadowPass = nullptr;
	struct ReflectiveShadowPass* reflectiveShadowPass = nullptr;
	struct SSAOPass* ssaoPass = nullptr;
	struct BlurPass* blurPass = nullptr;
	struct IndirectLightingRSMCompute* indirectLightingRSMCompute = nullptr;
	struct IndirectLightingAccumulationCompute* indirectLightingAccumulationCompute = nullptr;
	struct AtrousFilterPass* atrousFilterPass = nullptr;
	struct LightingPass* lightingPass = nullptr;
	struct TAAPass* taaPass = nullptr;
	struct SkyboxPass* skyboxPass = nullptr;
	struct TransparentPass* transparentPass = nullptr;
	struct TonemappingPass* tonemappingPass = nullptr;

	void Cleanup();
	void Setup();
	void Run(VkCommandBuffer cmd, uint32_t swapchainImageIndex);
	ResourceHandle CreateImage(std::string name, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
		bool mipmapped = false, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, VkImageCreateFlags createFlags = 0, uint32_t arrayLayers = 1);
	ResourceHandle CreateImage(std::string name, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
		bool mipmapped = false, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, VkImageCreateFlags createFlags = 0, uint32_t arrayLayers = 1);
	ResourceHandle CreateImageFromFile(const char* file, VkFormat format, VkImageUsageFlags usageFlags, bool mipmapped);
	ResourceHandle CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, std::string name);
	ResourceHandle CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, std::string name, VkMemoryPropertyFlags propertyFlags);
	ResourceHandle CreateCubemap(std::string paths[6]);
	MultiImageHandle CreateMultiImage();
	void AddDirtyResource(ResourceHandle* handle);
	void AddDirtyMultiImage(MultiImageHandle* handle);
	void AddSceneDataDependantPass(RenderPass* pass);
	void DestroyImage(const ResourceHandle& handle);
	void DestroyBuffer(const ResourceHandle& handle);
	void DestroyMultiImage(const MultiImageHandle& handle);

	void UpdateDescriptorSets();

	AllocatedBuffer& GetBuffer(const ResourceHandle& handle);
	AllocatedImage& GetImage(const ResourceHandle& handle);
	std::vector<VkDescriptorImageInfo>& GetMultiImage(const MultiImageHandle& handle);
private:
	std::set<ResourceHandle*> dirtyResources;
	std::set<MultiImageHandle*> dirtyMultiImages;

	std::vector<AllocatedImage> images;
	std::vector<AllocatedBuffer> buffers;
	std::vector<std::vector<VkDescriptorImageInfo>> multiImages;
	std::vector<RenderPass*> sceneDataDependantPasses;
};