﻿#include <vk_initializers.h>

//> init_cmd
VkCommandPoolCreateInfo vkinit::command_pool_create_info(uint32_t queueFamilyIndex,
    VkCommandPoolCreateFlags flags /*= 0*/)
{
    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = nullptr;

    info.flags = flags;
    return info;
}


VkCommandBufferAllocateInfo vkinit::command_buffer_allocate_info(
    VkCommandPool pool, uint32_t _count /*= 1*/, VkCommandBufferLevel level /*=VK_COMMAND_BUFFER_LEVEL_PRIMARY*/)
{
    VkCommandBufferAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = nullptr;

    info.commandPool = pool;
    info.commandBufferCount = _count;
    info.level = level;
    return info;
}

VkCommandBufferInheritanceInfo vkinit::command_buffer_inheritance_info()
{
    VkCommandBufferInheritanceInfo inheritanceInfo = {};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.pNext = nullptr;
    // Set other inheritance properties as needed
    inheritanceInfo.renderPass = VK_NULL_HANDLE; // This depends on whether you are recording for a render pass
    inheritanceInfo.subpass = 0; // Typically set to 0 unless you are in a multi-subpass situation
    inheritanceInfo.framebuffer = VK_NULL_HANDLE; // Typically set to VK_NULL_HANDLE unless recording for a specific framebuffer

    return inheritanceInfo;
}

//< init_cmd
// 
//> init_cmd_draw
VkCommandBufferBeginInfo vkinit::command_buffer_begin_info(VkCommandBufferUsageFlags flags /*= 0*/, VkCommandBufferInheritanceInfo* inheritanceInfo /*= nullptr*/)
{
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;
    info.pInheritanceInfo = inheritanceInfo;
    info.flags = flags;
    return info;
}
//< init_cmd_draw

//> init_sync
VkFenceCreateInfo vkinit::fence_create_info(VkFenceCreateFlags flags /*= 0*/)
{
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = nullptr;

    info.flags = flags;

    return info;
}

VkSemaphoreCreateInfo vkinit::semaphore_create_info(VkSemaphoreCreateFlags flags /*= 0*/)
{
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}
//< init_sync

//> init_submit
VkSemaphoreSubmitInfo vkinit::semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
	VkSemaphoreSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.semaphore = semaphore;
	submitInfo.stageMask = stageMask;
	submitInfo.deviceIndex = 0;
	submitInfo.value = 1;

	return submitInfo;
}

VkCommandBufferSubmitInfo vkinit::command_buffer_submit_info(VkCommandBuffer cmd)
{
	VkCommandBufferSubmitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	info.pNext = nullptr;
	info.commandBuffer = cmd;
	info.deviceMask = 0;

	return info;
}

VkSubmitInfo2 vkinit::submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
    VkSemaphoreSubmitInfo* waitSemaphoreInfo, uint32_t commandBufferCount /*= 1*/)
{
    VkSubmitInfo2 info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    info.pNext = nullptr;

    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = commandBufferCount;
    info.pCommandBufferInfos = cmd;

    return info;
}
//< init_submit

VkPresentInfoKHR vkinit::present_info()
{
    VkPresentInfoKHR info = {};
    info.sType =  VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pNext = 0;

    info.swapchainCount = 0;
    info.pSwapchains = nullptr;
    info.pWaitSemaphores = nullptr;
    info.waitSemaphoreCount = 0;
    info.pImageIndices = nullptr;

    return info;
}

//> color_info
VkRenderingAttachmentInfo vkinit::attachment_info(
    VkImageView view, VkClearValue* Clear ,VkImageLayout _layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/)
{
    VkRenderingAttachmentInfo colorAttachment {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.pNext = nullptr;

    colorAttachment.imageView = view;
    colorAttachment.imageLayout = _layout;
    colorAttachment.loadOp = Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (Clear) {
        colorAttachment.clearValue = *Clear;
    }

    return colorAttachment;
}
//< color_info
//> depth_info
VkRenderingAttachmentInfo vkinit::depth_attachment_info(
    VkImageView view, VkImageLayout _layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/, VkClearDepthStencilValue* clearValue, bool clear)
{
    VkRenderingAttachmentInfo depthAttachment {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.pNext = nullptr;

    depthAttachment.imageView = view;
    depthAttachment.imageLayout = _layout;
    depthAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil = clearValue ? *clearValue : VkClearDepthStencilValue{ 0.0f, 0 };

    return depthAttachment;
}
//< depth_info
//> render_info
VkRenderingInfo vkinit::rendering_info(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment,
    VkRenderingAttachmentInfo* depthAttachment, VkRenderingAttachmentInfo* stencilAttachment)
{
    VkRenderingInfo renderInfo {};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;

    renderInfo.renderArea = VkRect2D { VkOffset2D { 0, 0 }, renderExtent };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = colorAttachment;
    renderInfo.pDepthAttachment = depthAttachment;
    renderInfo.pStencilAttachment = stencilAttachment;

    return renderInfo;
}
//< render_info
//> subresource
VkImageSubresourceRange vkinit::image_subresource_range(VkImageAspectFlags aspectMask)
{
    VkImageSubresourceRange subImage {};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return subImage;
}
//< subresource



VkDescriptorSetLayoutBinding vkinit::descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags,
    uint32_t binding)
{
    VkDescriptorSetLayoutBinding setbind = {};
    setbind.binding = binding;
    setbind.descriptorCount = 1;
    setbind.descriptorType = type;
    setbind.pImmutableSamplers = nullptr;
    setbind.stageFlags = stageFlags;

    return setbind;
}

VkDescriptorSetLayoutCreateInfo vkinit::descriptorset_layout_create_info(VkDescriptorSetLayoutBinding* _bindings,
    uint32_t bindingCount)
{
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = nullptr;

    info.pBindings = _bindings;
    info.bindingCount = bindingCount;
    info.flags = 0;

    return info;
}

VkWriteDescriptorSet vkinit::write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet,
    VkDescriptorImageInfo* imageInfo, uint32_t binding)
{
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;

    write.dstBinding = binding;
    write.dstSet = dstSet;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = imageInfo;

    return write;
}

VkWriteDescriptorSet vkinit::write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet,
    VkDescriptorBufferInfo* bufferInfo, uint32_t binding)
{
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;

    write.dstBinding = binding;
    write.dstSet = dstSet;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = bufferInfo;

    return write;
}

VkDescriptorBufferInfo vkinit::buffer_info(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
    VkDescriptorBufferInfo binfo {};
    binfo.buffer = buffer;
    binfo.offset = offset;
    binfo.range = range;
    return binfo;
}

//> image_set
VkImageCreateInfo vkinit::image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, VkImageCreateFlags createFlags /*=0*/)
{
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;

    info.imageType = VK_IMAGE_TYPE_2D;

    info.format = format;
    info.extent = extent;

    info.mipLevels = 1;
    info.arrayLayers = 1;

    //for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
    info.samples = VK_SAMPLE_COUNT_1_BIT;

    //optimal tiling, which means the image is stored on the best gpu format
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usageFlags;
    info.flags = createFlags;

    return info;
}

VkImageViewCreateInfo vkinit::imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, VkImageViewType viewType)
{
    // build a image-view for the depth image to use for rendering
    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = nullptr;

    info.viewType = viewType;
    info.image = image;
    info.format = format;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.aspectMask = aspectFlags;

    return info;
}
//< image_set
VkPipelineLayoutCreateInfo vkinit::pipeline_layout_create_info()
{
    VkPipelineLayoutCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = nullptr;

    // empty defaults
    info.flags = 0;
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;
    return info;
}

VkPipelineShaderStageCreateInfo vkinit::pipeline_shader_stage_create_info(VkShaderStageFlagBits stage,
    VkShaderModule shaderModule,
    const char * entry)
{
    VkPipelineShaderStageCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.pNext = nullptr;

    // shader stage
    info.stage = stage;
    // module containing the code for this shader stage
    info.module = shaderModule;
    // the entry point of the shader
    info.pName = entry;
    return info;
}

VkBufferMemoryBarrier2 vkinit::pipeline_buffer_memory_barrier(
    VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, 
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkBuffer buffer, uint32_t size)
{
    VkBufferMemoryBarrier2 memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = srcStageMask;
    memoryBarrier.dstStageMask = dstStageMask;
    memoryBarrier.srcAccessMask = srcAccessMask;
    memoryBarrier.dstAccessMask = dstAccessMask;
    memoryBarrier.buffer = buffer;
    memoryBarrier.offset = 0;
    memoryBarrier.size = size;
    
    return memoryBarrier;
}

VkImageMemoryBarrier2 vkinit::pipeline_image_memory_barrier(
    VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkImage image, VkImageAspectFlags aspectFlags)
{
    VkImageMemoryBarrier2 memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = srcStageMask;
    memoryBarrier.dstStageMask = dstStageMask;
    memoryBarrier.srcAccessMask = srcAccessMask;
    memoryBarrier.dstAccessMask = dstAccessMask;
    memoryBarrier.image = image;
    memoryBarrier.subresourceRange.aspectMask = aspectFlags;
    memoryBarrier.subresourceRange.layerCount = 1;
    memoryBarrier.subresourceRange.levelCount = 1;
    return memoryBarrier;
}