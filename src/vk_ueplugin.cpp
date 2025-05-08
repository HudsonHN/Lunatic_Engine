#include "vk_ueplugin.h"

void AddVertex(const Vertex& vertex)
{
	engineLink->_allVertices.push_back(vertex);	
}

void AddIndex(uint32_t index)
{
	engineLink->_allIndices.push_back(index);	
}

void UpdateTransform(uint32_t index, glm::mat4 mat)
{
    engineLink->_allSurfaceMetaInfo[index].worldTransform = mat;
}

uint32_t GetTotalVertexCount()
{
    return static_cast<uint32_t>(engineLink->_allVertices.size());
}

uint32_t GetTotalIndexCount()
{
    return static_cast<uint32_t>(engineLink->_allIndices.size());
}

LUNATIC_API void AddIndirectDraw(const GeoSurface& surface)
{
    SurfaceMetaInfo metaInfo;
    metaInfo.surfaceIndex = engineLink->_totalDrawCounter; // THIS MUST CORRESPOND WITH THE DRAW COMMAND FIRST INSTANCE
    metaInfo.materialIndex = surface.materialIndex;
    metaInfo.bounds = surface.bounds;
    metaInfo.worldTransform = surface.worldTransform;
    metaInfo.bIsTransparent = surface.materialType == MaterialPass::Transparent;
    metaInfo.drawIndex = surface.materialType == MaterialPass::Transparent ? engineLink->_transparentDrawCounter : engineLink->_opaqueDrawCounter;

    engineLink->_allSurfaceMetaInfo.push_back(metaInfo);

    VkDrawIndexedIndirectCommand drawCommand{};
    drawCommand.indexCount = surface.indexCount; // How many indices to iterate through in this draw
    drawCommand.instanceCount = 1;
    drawCommand.firstIndex = surface.indexOffset; // firstIndex is indexOffset, aka where the index for this draw starts in the global index buffer
    drawCommand.vertexOffset = 0;
    drawCommand.firstInstance = engineLink->_totalDrawCounter;

    switch (surface.materialType)
    {
        case MaterialPass::MainColor:
        {
            engineLink->_allOpaqueDrawCommands.push_back(drawCommand);
            engineLink->_opaqueDrawCounter++;
            break;
        }
        case MaterialPass::Transparent:
        {
            engineLink->_allTransparentDrawCommands.push_back(drawCommand);
            engineLink->_transparentDrawCounter++;
            break;
        }
        default:
        {
            engineLink->_allOpaqueDrawCommands.push_back(drawCommand);
            engineLink->_opaqueDrawCounter++;
            break;
        }
    }
    engineLink->_totalDrawCounter++;
}

void AddMaterial(const MaterialConstants& constants)
{
    MaterialResources materialResources;
    // Default the material textures
    materialResources.colorImage = queuedImages.front();
    queuedImages.pop();
    materialResources.colorSampler = engineLink->_defaultSamplerLinear;
    materialResources.metalRoughImage = queuedImages.front();
    queuedImages.pop();
    materialResources.metalRoughSampler = engineLink->_defaultSamplerLinear;
    materialResources.normalImage = queuedImages.front();
    queuedImages.pop();
    materialResources.normalSampler = engineLink->_defaultSamplerLinear;
    materialResources.constants = constants;

    engineLink->WriteMaterial(engineLink->_device, materialResources, engineLink->_globalDescriptorAllocator);
}
void AddTexture(void* data, uint32_t width, uint32_t height)
{
    if (data == nullptr)
    {
        queuedImages.push(engineLink->_renderGraph.GetImage(engineLink->_whiteImage));
        return;
    }

    VkExtent3D extent =
    {
        .width = width,
        .height = height,
        .depth = 1
    };
    AllocatedImage image = engineLink->CreateImage("UE_texture", data, extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);
    queuedImages.push(image);

    engineLink->_mainDeletionQueue.PushFunction([=]() {
        engineLink->DestroyImage(image);
    });
}

void InitInstance()
{ 
    engineLink = new LunaticEngine();
    engineLink->_bIsLinkedToUnreal = true;
}

void InitEngine()
{
	engineLink->Init();
    engineLink->_bBiasShadowMaps = true;
}

void UploadStream()
{
    engineLink->UploadMeshes();
}

void RunEngine()
{
	engineLink->UERun();
}
void CloseEngine()
{
	engineLink->Cleanup();
    delete engineLink;
    engineLink = nullptr;
}

void ChangeSkybox(bool check)
{
    if (check)
    {
        engineLink->_cubeMapPaths[0] = ASSET_PATH"\\Textures\\Yokohama3\\posx.jpg"; // Right
        engineLink->_cubeMapPaths[1] = ASSET_PATH"\\Textures\\Yokohama3\\negx.jpg"; // Left
        engineLink->_cubeMapPaths[2] = ASSET_PATH"\\Textures\\Yokohama3\\posy.jpg"; // Top
        engineLink->_cubeMapPaths[3] = ASSET_PATH"\\Textures\\Yokohama3\\negy.jpg"; // Bottom
        engineLink->_cubeMapPaths[4] = ASSET_PATH"\\Textures\\Yokohama3\\posz.jpg"; // Front
        engineLink->_cubeMapPaths[5] = ASSET_PATH"\\Textures\\Yokohama3\\negz.jpg"; // Back
    }
}