// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

namespace vkinit {
//> init_cmd
VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t _count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
VkCommandBufferInheritanceInfo command_buffer_inheritance_info();
//< init_cmd

VkBufferMemoryBarrier2 pipeline_buffer_memory_barrier(VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask, 
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkBuffer buffer, uint32_t size);

VkImageMemoryBarrier2 pipeline_image_memory_barrier(VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask,
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkImage image, VkImageAspectFlags aspectFlags);

VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0, VkCommandBufferInheritanceInfo* inheritanceInfo = nullptr);
VkCommandBufferSubmitInfo command_buffer_submit_info(VkCommandBuffer cmd);

VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

VkSubmitInfo2 submit_info(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
    VkSemaphoreSubmitInfo* waitSemaphoreInfo, uint32_t commandBufferCount = 1);
VkPresentInfoKHR present_info();

VkRenderingAttachmentInfo attachment_info(VkImageView view, VkClearValue* Clear, VkImageLayout _layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

VkRenderingAttachmentInfo depth_attachment_info(VkImageView view,
    VkImageLayout _layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/, VkClearDepthStencilValue* clearValue = nullptr, bool clear = true);

VkRenderingInfo rendering_info(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment,
    VkRenderingAttachmentInfo* depthAttachment, VkRenderingAttachmentInfo* stencilAttachment);

VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);

VkSemaphoreSubmitInfo semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
VkDescriptorSetLayoutBinding descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags,
    uint32_t binding);
VkDescriptorSetLayoutCreateInfo descriptorset_layout_create_info(VkDescriptorSetLayoutBinding* _bindings,
    uint32_t bindingCount);
VkWriteDescriptorSet write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet,
    VkDescriptorImageInfo* imageInfo, uint32_t binding);
VkWriteDescriptorSet write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet,
    VkDescriptorBufferInfo* bufferInfo, uint32_t binding);
VkDescriptorBufferInfo buffer_info(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

VkImageCreateInfo image_create_info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, VkImageCreateFlags createFlags = 0);
VkImageViewCreateInfo imageview_create_info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);
VkPipelineLayoutCreateInfo pipeline_layout_create_info();
VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage,
    VkShaderModule shaderModule,
    const char * entry = "main");
} // namespace vkinit
