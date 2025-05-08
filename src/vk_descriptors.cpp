#include <vk_descriptors.h>

void DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shaderStages, uint32_t descriptorCount /*= 1*/)
{
    VkDescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorCount = descriptorCount;
    newbind.descriptorType = type;
    newbind.stageFlags = shaderStages;

    _bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::Clear()
{
    _bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VkDevice device, VkDescriptorSetLayoutCreateFlags createFlags /*= 0*/, VkDescriptorBindingFlags* bindingFlags /*= 0*/)
{
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingInfo{};
    bindingInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingInfo.pNext = nullptr;
    bindingInfo.bindingCount = (uint32_t)_bindings.size();
    bindingInfo.pBindingFlags = bindingFlags;

    VkDescriptorSetLayoutCreateInfo info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.pNext = bindingFlags != nullptr ? &bindingInfo : nullptr;
    info.pBindings = _bindings.data();
    info.bindingCount = (uint32_t)_bindings.size();
    info.flags = createFlags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

void DescriptorAllocator::InitPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio._type,
            .descriptorCount = uint32_t(ratio._ratio * maxSets)
            });
    }

    VkDescriptorPoolCreateInfo pool_info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = 0;
    pool_info.maxSets = maxSets;
    pool_info.poolSizeCount = (uint32_t)poolSizes.size();
    pool_info.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &pool_info, nullptr, &_pool);
}

void DescriptorAllocator::ClearDescriptors(VkDevice device)
{
    vkResetDescriptorPool(device, _pool, 0);
}

void DescriptorAllocator::DestroyPool(VkDevice device)
{
    vkDestroyDescriptorPool(device, _pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::Allocate(VkDevice device, VkDescriptorSetLayout _layout)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = _pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &_layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}

VkDescriptorSet DescriptorAllocator::Allocate(VkDevice device, VkDescriptorSetLayout _layout, std::vector<uint32_t> descriptorCounts)
{
    VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo{};
    countInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    countInfo.descriptorSetCount = 1;
    countInfo.pDescriptorCounts = descriptorCounts.data();
    countInfo.pNext = nullptr;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = descriptorCounts.data() == nullptr ? nullptr : &countInfo;
    allocInfo.descriptorPool = _pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &_layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::GetPool(VkDevice device)
{
    VkDescriptorPool newPool;
    if (_readyPools.size() != 0) {
        newPool = _readyPools.back();
        _readyPools.pop_back();
    }
    else {
        //need to create a new pool
        newPool = CreatePool(device, _setsPerPool, _ratios, _poolCreateFlags);

        _setsPerPool = static_cast<uint32_t>(_setsPerPool * 1.5);
        if (_setsPerPool > 4092) {
            _setsPerPool = 4092;
        }
    }

    return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::CreatePool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios, VkDescriptorPoolCreateFlags flags)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio._type,
            .descriptorCount = static_cast<uint32_t>(ratio._ratio * setCount)
        });
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.maxSets = setCount;
    pool_info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    pool_info.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &newPool);
    return newPool;
}

void DescriptorAllocatorGrowable::Init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios, VkDescriptorPoolCreateFlags flags /*= 0*/)
{
    _ratios.clear();

    for (auto r : poolRatios) {
        _ratios.push_back(r);
    }

    _poolCreateFlags = flags;

    VkDescriptorPool newPool = CreatePool(device, maxSets, poolRatios, flags);

    _setsPerPool = static_cast<uint32_t>(maxSets * 1.5); //grow it next allocation

    _lastUsedPool = newPool;
    _readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::ClearPools(VkDevice device)
{
    for (auto p : _readyPools) {
        vkResetDescriptorPool(device, p, 0);
    }
    for (auto p : _fullPools) {
        vkResetDescriptorPool(device, p, 0);
        _readyPools.push_back(p);
    }
    _fullPools.clear();
}

void DescriptorAllocatorGrowable::DestroyPools(VkDevice device)
{
    for (auto p : _readyPools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    _readyPools.clear();
    for (auto p : _fullPools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    _fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::Allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    //get or create a pool to allocate from
    VkDescriptorPool poolToUse = GetPool(device);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    //allocation failed. Try again
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

        _fullPools.push_back(poolToUse);

        poolToUse = GetPool(device);
        allocInfo.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }
    _lastUsedPool = poolToUse;
    _readyPools.push_back(poolToUse);
    return ds;
}

VkDescriptorSet DescriptorAllocatorGrowable::Allocate(VkDevice device, VkDescriptorSetLayout layout, uint32_t descriptorCounts)
{
    VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo{};
    countInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    countInfo.descriptorSetCount = 1;
    countInfo.pDescriptorCounts = &descriptorCounts;
    countInfo.pNext = nullptr;

    //get or create a pool to allocate from
    VkDescriptorPool poolToUse = GetPool(device);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = &countInfo;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    //allocation failed. Try again
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

        _fullPools.push_back(poolToUse);

        poolToUse = GetPool(device);
        allocInfo.descriptorPool = poolToUse;

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }
    _lastUsedPool = poolToUse;
    _readyPools.push_back(poolToUse);
    return ds;
}

void DescriptorWriter::WriteBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
    VkDescriptorBufferInfo& info = _bufferInfos.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = size
        });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;

    _writes.push_back(write);
}

void DescriptorWriter::WriteImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout _layout, VkDescriptorType type)
{
    VkDescriptorImageInfo& info = _imageInfos.emplace_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = image,
        .imageLayout = _layout
        });

    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &info;

    _writes.push_back(write);
}

void DescriptorWriter::WriteImages(int binding, const std::span<VkDescriptorImageInfo>& imageInfo, VkDescriptorType type)
{
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = static_cast<uint32_t>(imageInfo.size());
    write.descriptorType = type;
    write.pImageInfo = imageInfo.data();
    _writes.push_back(write);
}

void DescriptorWriter::Clear()
{
    _imageInfos.clear();
    _writes.clear();
    _bufferInfos.clear();
}

void DescriptorWriter::UpdateSet(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : _writes) 
    {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(_writes.size()), _writes.data(), 0, nullptr);
}