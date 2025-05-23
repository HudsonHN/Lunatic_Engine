#pragma once
#include <vulkan/vulkan.h>

namespace vkdebug
{
    void LoadDebugLabelFunctions(VkInstance instance);

    extern PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
    extern PFN_vkCmdEndDebugUtilsLabelEXT   vkCmdEndDebugUtilsLabelEXT;
}

class GPUDebugScope
{
public:
    GPUDebugScope(VkCommandBuffer cmd, const char* label);
    ~GPUDebugScope();
private:
    const VkCommandBuffer commandBuffer;
};