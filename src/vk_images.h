
#pragma once 

namespace vkutil {
    void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
    void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
    void GenerateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
    void* LoadImageDataFile(std::string path, int& width, int& height, int& channel);
    void FreeLoadedImage(void* data);
};