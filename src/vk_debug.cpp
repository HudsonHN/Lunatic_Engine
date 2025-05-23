#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "vk_debug.h"

namespace vkdebug 
{
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT = nullptr;

    void LoadDebugLabelFunctions(VkInstance instance) 
    {
        vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
        vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
    }
}

GPUDebugScope::GPUDebugScope(VkCommandBuffer cmd, const char* label) 
: commandBuffer(cmd)
{
    VkDebugUtilsLabelEXT labelInfo{};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = label;
    labelInfo.color[0] = 0.25f;
    labelInfo.color[1] = 0.6f;
    labelInfo.color[2] = 1.0f;
    labelInfo.color[3] = 1.0f;
    vkdebug::vkCmdBeginDebugUtilsLabelEXT(cmd, &labelInfo);
}

GPUDebugScope::~GPUDebugScope()
{
    vkdebug::vkCmdEndDebugUtilsLabelEXT(commandBuffer);
}