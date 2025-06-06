#pragma once
#include <unordered_map>
#include <filesystem>
#include "vk_types.h"
#include "vk_descriptors.h"
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

class LunaticEngine;

struct GLTFMaterial 
{
    MaterialPass matPassType;
};

struct GeoSurface 
{
    glm::mat4 worldTransform;
    MaterialPass materialType;
    std::string name;
    Bounds bounds;
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t materialIndex;
    uint32_t nameHash;
};

struct MeshAsset 
{
    std::string name;
    std::vector<GeoSurface> surfaces;
    uint32_t useCount;
};

struct MeshNode : public Node
{
    LunaticEngine* _engine;
    std::shared_ptr<MeshAsset> _mesh;
    std::unordered_map<uint32_t, uint32_t> _indices;
    virtual void Draw(const glm::mat4& topMatrix) override;
};

struct MaterialConstants
{
    glm::vec4 colorFactors;
    glm::vec4 metalRoughFactors;
};

struct MaterialResources
{
    AllocatedImage colorImage;
    VkSampler colorSampler;
    AllocatedImage metalRoughImage;
    VkSampler metalRoughSampler;
    AllocatedImage normalImage;
    VkSampler normalSampler;
    MaterialConstants constants;
};

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes(LunaticEngine* engine, std::filesystem::path filePath);

struct LoadedGLTF : public IRenderable 
{

    // storage for all the data on a given glTF file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> _meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> _nodes;
    std::vector<AllocatedImage> _images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> _materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> _topNodes;

    std::vector<VkSampler> _samplers;

    DescriptorAllocatorGrowable _descriptorPool;

    glm::mat4 _worldTransform;

    LunaticEngine* _engineInstance;

    ~LoadedGLTF() { ClearAll(); };

    virtual void Draw(const glm::mat4& topMatrix);

private:
    void ClearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> LoadGltf(LunaticEngine* engine, std::string_view filePath);

std::optional<AllocatedImage> LoadImage(LunaticEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);