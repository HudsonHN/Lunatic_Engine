#pragma once

#include <vk_types.h>

struct DescriptorLayoutBuilder {
    std::vector<VkDescriptorSetLayoutBinding> _bindings;

    void AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shaderStages, uint32_t descriptorCount = 1);
    void Clear();
    VkDescriptorSetLayout Build(VkDevice device, VkDescriptorSetLayoutCreateFlags createFlags = 0, VkDescriptorBindingFlags* bindingFlags = nullptr);
};

struct DescriptorAllocator {

    struct PoolSizeRatio {
        VkDescriptorType _type;
        float _ratio;
    };

    VkDescriptorPool _pool;

    void InitPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
    void ClearDescriptors(VkDevice device);
    void DestroyPool(VkDevice device);

    VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout, std::vector<uint32_t> descriptorCounts);
    VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout);
};

struct DescriptorAllocatorGrowable {
public:
	struct PoolSizeRatio {
		VkDescriptorType _type;
		float _ratio;
	};

	void Init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios, VkDescriptorPoolCreateFlags flags = 0);
	void ClearPools(VkDevice device);
	void DestroyPools(VkDevice device);

    VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout _layout, uint32_t descriptorCounts);
    VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout _layout);

    VkDescriptorPool _lastUsedPool;
private:
	VkDescriptorPool GetPool(VkDevice device);
	VkDescriptorPool CreatePool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios, VkDescriptorPoolCreateFlags flags);

	std::vector<PoolSizeRatio> _ratios;
	std::vector<VkDescriptorPool> _fullPools;
	std::vector<VkDescriptorPool> _readyPools;
	uint32_t _setsPerPool;
    VkDescriptorPoolCreateFlags _poolCreateFlags;
};

struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> _imageInfos;
    std::deque<VkDescriptorBufferInfo> _bufferInfos;
    std::vector<VkWriteDescriptorSet> _writes;

    void WriteImages(int binding, const std::span<VkDescriptorImageInfo>& imageInfo, VkDescriptorType type);
    void WriteImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout _layout, VkDescriptorType type);
    void WriteBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

    void Clear();
    void UpdateSet(VkDevice device, VkDescriptorSet set);
};