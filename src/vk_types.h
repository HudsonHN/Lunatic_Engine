// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#define MAX_DRAWS 1024
#define MAX_TEXTURES 2048
#define MAX_LIGHTS 256
#define NUM_CASCADES 4

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

enum class MaterialPass :uint8_t {
    MainColor,
    Transparent,
    Other
};

struct Bounds
{
    glm::vec3 origin; // vec3s are 16-byte aligned
    float pad0;
    glm::vec3 extents;
    float sphereRadius;
};

struct DescriptorBindingInfo
{
    uint32_t binding;
    VkDescriptorType type;
    VkImageLayout imageLayout;
    VkSampler imageSampler;
    bool isImage;
    VkDescriptorSet descriptorSet;
};

struct MultiDescriptorBindingInfo
{
    uint32_t binding;
    VkDescriptorType type;
    VkDescriptorSet descriptorSet;
};

struct ResourceHandle
{
    std::vector<DescriptorBindingInfo> bindingInfos;
    int index = -1;
    uint32_t allocSize = 0;
};

struct MultiImageHandle
{
    std::vector<MultiDescriptorBindingInfo> bindingInfos;
    int index = -1;
};

struct DeletionQueue
{
    std::deque<std::function<void()>> _deletors;

    void PushFunction(std::function<void()>&& function) {
        _deletors.push_back(function);
    }

    void Flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = _deletors.rbegin(); it != _deletors.rend(); it++) {
            (*it)(); //call functors
        }

        _deletors.clear();
    }
};

struct FrustumCullConstants
{
    //Plane frustumPlanes[6];
    glm::mat4 viewProj;
};

struct AtrousFilterConstants
{
    float stepWidth;		// Grows each iteration (1.0, 2.0, 4.0, etc.)
    float phiColor;			// Controls sensitivity to color difference
    float phiNormal;		// Controls sensitivity to normal difference
    float phiPosition;		// Controls sensitivity to position difference
    glm::vec2 texelSize;	// 1.0 / resolution
};

struct CascadeConstants
{
    glm::mat4 viewProj;
    VkDeviceAddress vertexBuffer;
};

struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
    uint32_t imageSize;
    std::string name;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    uint32_t usedSize;
    std::string name;
};

struct Vertex {
    glm::vec3 _position;
    float _uvX;
    glm::vec3 _normal;
    float _uvY;
    glm::vec4 _color;
    glm::vec4 _tangent;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
    glm::vec3 cameraPos;
    uint32_t numLights;
    glm::vec2 jitterOffset;
    uint32_t bApplyTAA;
    float renderScale;
    float deltaTime;
    uint32_t frameIndex;
};

enum class LightType : uint32_t {
    Point,
    Directional,
    Cone,
    Area,
    Other
};

struct Light
{
    glm::vec3 position;
    float pad0;
    glm::vec3 color;
    float intensity;
    glm::vec3 direction;
    float radius;
    LightType type;
    float pad2[3];
};

// base class for a renderable dynamic object
class IRenderable {

    virtual void Draw(const glm::mat4& topMatrix) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable {

    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> _parent;
    std::vector<std::shared_ptr<Node>> _children;

    glm::mat4 _localTransform;
    glm::mat4 _worldTransform;
    std::string _name;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        _worldTransform = parentMatrix * _localTransform;
        for (auto c : _children) 
        {
            c->refreshTransform(_worldTransform);
        }
    }

    virtual void Draw(const glm::mat4& topMatrix) override
    {
        // draw children
        for (auto& c : _children) 
        {
            c->Draw(topMatrix);
        }
    }
};
