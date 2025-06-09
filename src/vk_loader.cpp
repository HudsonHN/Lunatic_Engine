
#include <vk_loader.h>
#include "stb_image.h"
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include <glm/gtx/quaternion.hpp>

VkFilter ExtractFilter(fastgltf::Filter filter);
VkSamplerMipmapMode ExtractMipmapMode(fastgltf::Filter filter);

uint32_t HashName(const std::string& name) 
{
    // Example: FNV-1a 32-bit
    uint32_t hash = 2166136261u;
    for (char c : name) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

// DO NOT USE THIS ONE, INCOMPLETE
std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes(LunaticEngine* engine, std::filesystem::path filePath)
{
    std::cout << "Loading GLTF mesh: " << filePath << std::endl;

    fastgltf::GltfDataBuffer _data;
    _data.loadFromFile(filePath);

    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers
        | fastgltf::Options::LoadExternalBuffers;

    fastgltf::Asset gltf;
    fastgltf::Parser parser{};

    auto load = parser.loadBinaryGLTF(&_data, filePath.parent_path(), gltfOptions);
    if (load) {
        gltf = std::move(load.get());
    }
    else {
        fmt::println("Failed to load glTF mesh: {} \n", fastgltf::to_underlying(load.error()));
        return {};
    }
    std::vector<std::shared_ptr<MeshAsset>> _meshes;

    // use the same vectors for all meshes so that the memory doesnt reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        MeshAsset newmesh;

        newmesh.name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.indexOffset = (uint32_t)indices.size();
            newSurface.indexCount = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        indices.push_back(static_cast<uint32_t>(idx + initial_vtx));
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                newvtx._position = v;
                newvtx._normal = { 1, 0, 0 };
                newvtx._color = glm::vec4{ 1.f };
                newvtx._uvX = 0;
                newvtx._uvY = 0;
                vertices[initial_vtx + index] = newvtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initial_vtx + index]._normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index]._uvX = v.x;
                vertices[initial_vtx + index]._uvY = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index]._color = v;
                    });
            }
            newmesh.surfaces.push_back(newSurface);
        }

        // display the vertex normals
        constexpr bool OverrideColors = false;
        if (OverrideColors) {
            for (Vertex& vtx : vertices) {
                vtx._color = glm::vec4(vtx._normal, 1.f);
            }
        }
        _meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
    }

    return _meshes;
}

std::optional<std::shared_ptr<LoadedGLTF>> LoadGltf(LunaticEngine* engine, std::string_view filePath)
{
    fmt::println("Loading GLTF: {}", filePath);

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->_engineInstance = engine;
    LoadedGLTF& file = *scene.get();

    fastgltf::Parser parser{};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | 
        fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;

    fastgltf::GltfDataBuffer _data;
    _data.loadFromFile(filePath);

    fastgltf::Asset gltf;

    std::filesystem::path path = filePath;

    auto type = fastgltf::determineGltfFileType(&_data);
    if (type == fastgltf::GltfType::glTF)
    {
        auto load = parser.loadGLTF(&_data, path.parent_path(), gltfOptions);
        if (load) 
        {
            gltf = std::move(load.get());
        }
        else
        {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else if (type == fastgltf::GltfType::GLB)
    {
        auto load = parser.loadBinaryGLTF(&_data, path.parent_path(), gltfOptions);
        if (load) 
        {
            gltf = std::move(load.get());
        }
        else 
        {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    }
    else 
    {
        std::cerr << "Failed to determine glTF container" << std::endl;
        return {};
    }
    // we can estimate the descriptors we will need accurately
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };

    file._descriptorPool.Init(engine->_device, static_cast<uint32_t>(gltf.materials.size()), sizes);

    // load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers) 
    {

        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr };
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = ExtractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = ExtractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode = ExtractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->_device, &sampl, nullptr, &newSampler);

        file._samplers.push_back(newSampler);
    }

    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> tempMeshes;
    std::vector<std::shared_ptr<Node>> tempNodes;
    std::vector<AllocatedImage> tempImages;

    // load all textures
    for (fastgltf::Image& image : gltf.images) 
    {
        std::optional<AllocatedImage> img = LoadImage(engine, gltf, image);

        if (img.has_value()) 
        {
            tempImages.push_back(*img);
            file._images.push_back(*img);
            //fmt::println("Loaded image: {}", image.name.c_str());
        }
        else 
        {
            // we failed to load, so lets give the slot a backup texture to not
            // completely break loading
            tempImages.push_back(engine->_renderGraph.GetImage(engine->_errorCheckerboardImage));
            std::cout << "gltf failed to load texture " << image.name << std::endl;
        }
    }

    uint32_t prevMaterialSize = static_cast<uint32_t>(engine->_allMaterials.size());
    uint32_t prevMaterialInfoSize = static_cast<uint32_t>(engine->_renderGraph.GetMultiImage(engine->_allMaterialImages).size());
    for (fastgltf::Material& mat : gltf.materials)
    {
        std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
        engine->_allMaterials.push_back(newMat);
        file._materials[mat.name.c_str()] = newMat;

        MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metalRoughFactors.x = mat.pbrData.metallicFactor;
        constants.metalRoughFactors.y = mat.pbrData.roughnessFactor;

        // write material parameters to buffer
        MaterialPass _matPassType = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend)
        {
            _matPassType = MaterialPass::Transparent;
        }

        AllocatedImage& whiteImage = engine->_renderGraph.GetImage(engine->_whiteImage);

        MaterialResources materialResources;
        materialResources.constants = constants;
        // default the material textures
        materialResources.colorImage = whiteImage;
        materialResources.colorSampler = engine->_defaultSamplerLinear;
        materialResources.metalRoughImage = whiteImage;
        materialResources.metalRoughSampler = engine->_defaultSamplerLinear;
        materialResources.normalImage = whiteImage;
        materialResources.normalSampler = engine->_defaultSamplerLinear;

        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value())
        {
            size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            // fmt::println("mat name: {}, img index: {}, sampler index: {}", mat.name.c_str(), img, sampler);

            materialResources.colorImage = tempImages[img];
            materialResources.colorSampler = file._samplers[sampler];
        }
        if (mat.pbrData.metallicRoughnessTexture.has_value())
        {
            size_t img = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.value();

            materialResources.metalRoughImage = tempImages[img];
            materialResources.metalRoughSampler = file._samplers[sampler];
        }
        if (mat.normalTexture.has_value())
        {
            size_t img = gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.value();
            size_t sampler = gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.value();

            materialResources.normalImage = tempImages[img];
            materialResources.normalSampler = file._samplers[sampler];
        }
        // fmt::println("Building {}", mat.name.c_str());
        // build material
        newMat->matPassType = _matPassType;
        engine->WriteMaterial(engine->_device, materialResources, file._descriptorPool);
    }

    // Load all animations
    /*for (fastgltf::Animation& anim : gltf.animations)
    {

    }*/

    for (fastgltf::Mesh& mesh : gltf.meshes)
    {
        std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
        tempMeshes.push_back(newmesh);
        file._meshes[mesh.name.c_str()] = newmesh;
        newmesh->name = mesh.name;
        newmesh->useCount = 0;

        std::vector<Vertex> tempVertices;

        uint32_t primitiveIndex = 0;
        for (auto&& p : mesh.primitives) 
        {
            tempVertices.clear();
            GeoSurface& newSurface = newmesh->surfaces.emplace_back(GeoSurface{});
            newSurface.indexOffset = (uint32_t)engine->_allIndices.size();
            newSurface.indexCount = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;
            newSurface.name = std::format("{}_{}", mesh.name, primitiveIndex);
            newSurface.nameHash = HashName(newSurface.name);

            uint32_t initial_vtx = static_cast<uint32_t>(engine->_allVertices.size());

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        engine->_allIndices.push_back(idx + initial_vtx); // Offset the value of the index itself because 
                                                                        // we need it to access the correct part of the global vertex buffer
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                engine->_allVertices.resize(engine->_allVertices.size() + posAccessor.count);
                tempVertices.resize(posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index)
                    {
                        Vertex newvtx;
                        newvtx._position = v;
                        newvtx._normal = { 1, 0, 0 };
                        newvtx._color = glm::vec4{ 1.f };
                        newvtx._uvX = 0;
                        newvtx._uvY = 0;
                        newvtx._tangent = glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };
                        engine->_allVertices[initial_vtx + index] = newvtx;
                        tempVertices[index] = newvtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end())
            {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) 
                    {
                        engine->_allVertices[initial_vtx + index]._normal = v;
                        tempVertices[index]._normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) 
            {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, size_t index)
                    {
                        engine->_allVertices[initial_vtx + index]._uvX = v.x;
                        engine->_allVertices[initial_vtx + index]._uvY = v.y;
                        tempVertices[index]._uvX = v.x;
                        tempVertices[index]._uvY = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) 
            {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) 
                    {
                        engine->_allVertices[initial_vtx + index]._color = v;
                        tempVertices[index]._color = v;
                    });
            }

            auto tangents = p.findAttribute("TANGENT");
            if (tangents != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*tangents).second],
                    [&](glm::vec4 v, size_t index)
                    {
                        engine->_allVertices[initial_vtx + index]._tangent = v;
                        tempVertices[index]._tangent = v;
                    });
            }

            if (p.materialIndex.has_value()) 
            {
                newSurface.materialType = engine->_allMaterials[p.materialIndex.value() + prevMaterialSize]->matPassType;
                // We have to multiply by the index by 3 because our texture buffer will store both color, metalroughness, and normal side-by-side
                // so index = color, index + 1 = metallic, index + 2 = normal
                newSurface.materialIndex = (static_cast<uint32_t>(p.materialIndex.value()) * 3) + prevMaterialInfoSize;
            }
            else 
            {
                //newSurface.material = engine->_allMaterials[0];
                newSurface.materialType = engine->_allMaterials[0]->matPassType;
                newSurface.materialIndex = 0;
            }

            glm::vec3 minPos = tempVertices[0]._position;
            glm::vec3 maxPos = tempVertices[0]._position;
            for (size_t i = 0; i < tempVertices.size(); i++)
            {
                minPos = glm::min(minPos, tempVertices[i]._position);
                maxPos = glm::max(maxPos, tempVertices[i]._position);
            }

            // Calculate origin and extents from the min/max and use extent length for radius
            newSurface.bounds.origin = (maxPos + minPos) / 2.0f;
            newSurface.bounds.extents = (maxPos - minPos) / 2.0f;
            //newSurface.bounds.minPos = minPos;
            //newSurface.bounds.maxPos = maxPos;
            newSurface.bounds.sphereRadius = glm::length((maxPos - minPos) / 2.0f);
            
            primitiveIndex++;
        }

    }

    // load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes) 
    {
        std::shared_ptr<Node> newNode;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value()) 
        {
            newNode = std::make_shared<MeshNode>();
            std::shared_ptr<MeshAsset> tempMesh = tempMeshes[*node.meshIndex];
            static_cast<MeshNode*>(newNode.get())->_mesh = tempMesh;
            static_cast<MeshNode*>(newNode.get())->_engine = engine;
            newNode->_name = std::format("{}_{}", tempMesh->name, tempMesh->useCount);
            tempMesh->useCount++;
            engine->SetupMesh(static_cast<MeshNode*>(newNode.get()));
        }
        else 
        {
            newNode = std::make_shared<Node>();
        }

        tempNodes.push_back(newNode);

        file._nodes[node.name.c_str()] = newNode;

        std::visit(fastgltf::visitor{ [&](fastgltf::Node::TransformMatrix matrix) {
                                          memcpy(&newNode->_localTransform, matrix.data(), sizeof(matrix));
                                      },
                       [&](fastgltf::Node::TRS transform) {
                           glm::vec3 tl(transform.translation[0], transform.translation[1],
                               transform.translation[2]);
                           glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                               transform.rotation[2]);
                           glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                           glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                           glm::mat4 rm = glm::toMat4(rot);
                           glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                           newNode->_localTransform = tm * rm * sm;
                       } },
            node.transform);
    }

    // run loop again to setup transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++) 
    {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = tempNodes[i];

        for (auto& c : node.children) 
        {
            sceneNode->_children.push_back(tempNodes[c]);
            tempNodes[c]->_parent = sceneNode;
        }
    }

    // find the top nodes, with no parents
    for (auto& node : tempNodes) 
    {
        if (node->_parent.lock() == nullptr) 
        {
            file._topNodes.push_back(node);
            node->refreshTransform(glm::mat4{ 1.f });
        }
    }

    fmt::println("Finished loading GLTF: {}", filePath);
    return scene;
}

void LoadedGLTF::Draw(const glm::mat4& topMatrix)
{
    // create renderables from the scenenodes
    for (auto& n : _topNodes) 
    {
        n->Draw(topMatrix);
    }
}

void LoadedGLTF::ClearAll()
{
    VkDevice dv = _engineInstance->_device;

    _descriptorPool.DestroyPools(dv);
    // _engineInstance->DestroyBuffer(_materialDataBuffer);

    AllocatedImage& errorCheckerBoardImage = _engineInstance->_renderGraph.GetImage(_engineInstance->_errorCheckerboardImage);

    for (auto& v : _images) {

        if (v.image == errorCheckerBoardImage.image) {
            //dont destroy the default images
            continue;
        }
        _engineInstance->DestroyImage(v);
    }

    for (auto& sampler : _samplers) {
        vkDestroySampler(dv, sampler, nullptr);
    }

}

VkFilter ExtractFilter(fastgltf::Filter filter)
{
    switch (filter)
    {
        // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

        // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode ExtractMipmapMode(fastgltf::Filter filter)
{
    switch (filter) 
    {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

std::optional<AllocatedImage> LoadImage(LunaticEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
    AllocatedImage newImage {};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor {
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading
                                                    // local files.

                const std::string path(filePath.uri.path().begin(),
                    filePath.uri.path().end()); // Thanks C++.
                unsigned char* _data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if (_data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->CreateImage("path image", _data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(_data);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                unsigned char* _data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                    &width, &height, &nrChannels, 4);
                if (_data) {
                    VkExtent3D imageSize;
                    imageSize.width = width;
                    imageSize.height = height;
                    imageSize.depth = 1;

                    newImage = engine->CreateImage("vector image", _data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(_data);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(
                    fastgltf::visitor 
                    { 
                        // We only care about VectorWithMime here, because we specify LoadExternalBuffers,
                        // meaning all buffers are already loaded into a vector.
                        [](auto& arg) {},
                        [&](fastgltf::sources::Vector& vector) 
                        {
                            unsigned char* _data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
                                static_cast<int>(bufferView.byteLength),
                                &width, &height, &nrChannels, 4);
                            if (_data) {
                                VkExtent3D imagesize;
                                imagesize.width = width;
                                imagesize.height = height;
                                imagesize.depth = 1;

                                newImage = engine->CreateImage("vector image 2", _data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                    VK_IMAGE_USAGE_SAMPLED_BIT, true);

                                stbi_image_free(_data);
                            }
                        } 
                    },
                    buffer.data
                );
            },
        },
        image.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
        return {};
    } else {
        return newImage;
    }
}

void MeshNode::Draw(const glm::mat4& topMatrix)
{
    glm::mat4 nodeMatrix = topMatrix * _worldTransform;

    for (auto& s : _mesh->surfaces)
    {
        _engine->UpdateMesh(_indices[s.nameHash], nodeMatrix);
    }

    // recurse down
    Node::Draw(topMatrix);
}