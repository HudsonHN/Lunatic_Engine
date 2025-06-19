#pragma once

#include "camera.h"
#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "vk_rendergraph.h"

#define SHADER_PATH "..\\..\\shaders"
#define ASSET_PATH "..\\..\\assets"

constexpr unsigned int FRAME_OVERLAP = 3;

struct SkyboxConstants
{
	glm::mat4 viewProj;
};

struct FrameData 
{
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;

	// We use this semaphore to wait for the next image to be ready to be drawn onto
	VkSemaphore swapchainSemaphore;
	// We use this semaphore to wait for the image to be done drawing to present it to the screen
	VkSemaphore renderSemaphore;
	// We use this to make sure the CPU waits for the GPU to finish drawing
	VkFence renderFence;

	DeletionQueue deletionQueue;
	DescriptorAllocatorGrowable frameDescriptors;
};

struct EngineStats
{
	float frameTime;
	int triangleCount;
	int drawcallCount;
	float sceneUpdateTime;
	float meshDrawTime;
	float gpuDataTransferTime;
	float localProfileTime;
	float initializationTime;
	float cullingTime;
	float shadowPassTime;
	float opaquePassTime;
	float transparentPassTime;
};

struct Plane
{
	glm::vec3 normal;
	float distance;
};

struct IndirectDrawInfo
{
	uint32_t index;
	VkDrawIndexedIndirectCommand drawCommand;
};

struct ImageMetaInfo
{
	MaterialConstants constants;
};

// Needs to be aligned to 16 bytes as with any struct stored in an array in GPU
struct alignas(16) SurfaceMetaInfo
{
	glm::mat4 worldTransform;
	Bounds bounds;
	uint32_t surfaceIndex; // The global ID of the indirect draw across all indirect draws (opaque, transparent, etc.)
	uint32_t materialIndex;
	uint32_t drawIndex; // To index into the opaque and transparent draw arrays
	uint32_t bIsTransparent;
	uint32_t bIsSkinned;
	uint32_t boneOffset; // Byte offset into the global bone transform buffer
	uint32_t boneCount; // Number of joints the surface uses
};

struct SSAOConstants
{
	glm::vec2 screenResolution;
	float radius;
	float bias;
	uint32_t kernelSize;
	float power;
	uint32_t pad[2];
};

struct DeferredLightingConstants
{
	int bDrawShadowMaps;
	int bDrawReflectiveShadowMaps;
	int numIndirectLightSamples;
	int bDrawSsao;
	float sampleRadius;
	float reflectiveIntensity;
	float ambientScale;
	float minBiasFactor;
	float maxBiasFactor;
	float pad[3];
};

class LunaticEngine
{
public:
	FrameData& GetCurrentFrame() { return _frames[_frameNumber % FRAME_OVERLAP]; }

	bool _stopRendering = false;

	static LunaticEngine& Get();

	//initializes everything in the engine
	void Init();

	//shuts down the engine
	void Cleanup();

	//draw loop
	void Draw();

	//run main loop
	void Run();
	void RunImGui();
	bool shouldQuit = false;
	void TestRun();
	void UERun();

	void UpdateScene();
	void LoadMesh(const std::string& path, const glm::mat4& worldTransform);
	void UploadMeshes();
	void UploadRuntimeInfo();
	void UpdateMesh(uint32_t index, const glm::mat4& worldTransform);
	void SetupMesh(MeshNode* meshNode);
	void CreateLight(glm::vec3 position, glm::vec3 color, float intensity, float radius, LightType type);

	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, std::string name);
	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, std::string name, VkMemoryPropertyFlags propertyFlags);
	AllocatedBuffer ReadGPUBuffer(VkCommandBuffer cmd, AllocatedBuffer& gpuBuffer, uint32_t size);
	void DestroyBuffer(const AllocatedBuffer& buffer);
	void ResizeBufferGPU(AllocatedBuffer& currentBuffer, uint32_t prevSize, uint32_t newSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage);
	void ResizeBufferGPU(AllocatedBuffer& currentBuffer, uint32_t prevSize, uint32_t newSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags propertyFlags);
	
	AllocatedImage LoadCubemap(std::string paths[6]);
	
	AllocatedImage LoadImageFromFile(const char* file, VkFormat format, VkImageUsageFlags usageFlags, bool mipmapped = false, bool cubeMap=false);
	AllocatedImage CreateImage(std::string name, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, 
		bool mipmapped = false, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, VkImageCreateFlags createFlags = 0, uint32_t arrayLayers = 1);
	AllocatedImage CreateImage(std::string name, void* _data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, 
		bool mipmapped = false, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, VkImageCreateFlags createFlags = 0, uint32_t arrayLayers = 1);
	void DestroyImage(const AllocatedImage& img);
	void WriteMaterial(VkDevice device, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);

	std::unordered_map<std::string, AllocatedBuffer> _allocatedBuffers;
	std::unordered_map<std::string, AllocatedImage> _allocatedImages;

	FrameData _frames[FRAME_OVERLAP];
	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	VkExtent2D _windowExtent{ 1920, 1080 };

	struct SDL_Window* _window{ nullptr };

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;
	VkInstance _instance;// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;// Vulkan debug output handle
	VkPhysicalDevice _chosenGPU;// GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface;// Vulkan window surface

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;

	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;
	ResourceHandle _intermediateImage;

	DeletionQueue _mainDeletionQueue;

	VmaAllocator _allocator;
	ResourceHandle _drawImage;
	ResourceHandle _depthImage;
	VkExtent2D _drawExtent;

	DescriptorAllocatorGrowable _globalDescriptorAllocator;
	DescriptorAllocatorGrowable _bindlessDescriptorAllocator;
	FrustumCullConstants _frustumCullConstants;
	bool _bApplyTonemap = true;

	AtrousFilterConstants _atrousFilterConstants;
	int _atrousFilterNumSamples = 2;


	SSAOConstants _ssaoConstants;
	std::vector<glm::vec3> _ssaoKernel;
	float _ssaoBlurAmount;
	bool _bApplySSAO = true;
	bool _bApplySSAOBlur = true;

	bool _bApplyIndirectAccumulation = true;

	bool _bUseIndirectLightingCompute = true;
	bool _bApplyAtrousFilter = true;
	std::vector<glm::mat4> _inverseIndirLightViewProj;
	std::vector<glm::mat4> _indirLightViewProj;
	std::vector<glm::mat4> _dirLightViewProj;
	float _cascadeSplitLambda = 0.95f;
	float _shadowFadeRegion = 5.0f;
	const std::vector<glm::vec2> _cascadeNearFarPlanes =
	{
		glm::vec2(0.1f, 10.0f),
		glm::vec2(8.0f, 30.0f),
		glm::vec2(22.5f, 100.0f),
		glm::vec2(92.5f, 400.0f),
	};
	const std::vector<glm::vec2> _indirectCascadeNearFarPlanes =
	{
		glm::vec2(0.1f, 20.0f),
		glm::vec2(10.0f, 40.0f),
		glm::vec2(22.5f, 100.0f),
		glm::vec2(92.5f, 400.0f),
	};
	int _cascadeExtentIndex = 2;
	int _prevCascadeExtentIndex = 2;
	const VkExtent2D _cascadeExtents[3] = 
	{
		{ 1024, 1024 },
		{ 2048, 2048 },
		{ 4096, 4096 }
	};
	float _minBiasFactor = 0.001f;
	float _maxBiasFactor = 0.05f;
	bool _bBiasShadowMaps = false;
	bool _bDrawDirectionalShadows = true;
	bool _bDrawReflectiveDirectionalShadows = true;
	float _reflectiveIntensity = 35.0f;
	int _numReflectiveShadowSamples = 16;
	float _sampleRadius = 0.08f;
	float _ambientScale = 0.1f;

	MultiImageHandle _reflectiveShadowMapImages;
	MultiImageHandle _shadowMapImages;

	glm::vec2 _deltaTimeMinMaxWeight = glm::vec2(0.1f, 1.0f);
	float _jitterScale = 1.0f;
	const uint32_t _jitterCount = 16;
	std::vector<glm::vec2> _haltonJitterOffsets;

	bool _bApplyTAA = true;

	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

	ResourceHandle _whiteImage;
	ResourceHandle _blackImage;
	ResourceHandle _greyImage;
	ResourceHandle _errorCheckerboardImage;
	ResourceHandle _cubeMap;
	ResourceHandle _cascadeDepth[4];
	ResourceHandle _cascadeReflectDepth[4];
	ResourceHandle _cascadeNormal[4];
	ResourceHandle _cascadePosition[4];
	ResourceHandle _cascadeFlux[4];
	ResourceHandle _indirectLightColor;
	ResourceHandle _prevIndirectLightColor;
	ResourceHandle _historyIndirectLightColor;
	ResourceHandle _positionColor;
	ResourceHandle _normalColor;
	ResourceHandle _albedoSpecColor;
	ResourceHandle _metalRoughnessColor;
	ResourceHandle _noiseAOTexture;
	ResourceHandle _ssaoColor;
	ResourceHandle _velocityImage;
	ResourceHandle _prevVelocityImage;
	ResourceHandle _prevFrameImage;
	ResourceHandle _prevPositionImage;
	ResourceHandle _historyImage;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;
	VkSampler _defaultSamplerRepeat;

	VkDescriptorSetLayout _singleImageDescriptorLayout;
	
	GPUSceneData _sceneData;
	GPUSceneData _prevSceneData;

	Camera _mainCamera;
	
	float _renderScale = 1.0f;

	float _deltaTime = 0.0f;
	std::chrono::system_clock::time_point _lastTime;

	bool _resizeRequested = false;

	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> _loadedScenes;

	EngineStats _perfStats;
	DeferredLightingConstants _lightingConstants;

	std::string _cubeMapPaths[6] =
	{
		ASSET_PATH"\\Textures\\Skyboxes\\NiagaraFalls2\\posx.jpg", // Right
		ASSET_PATH"\\Textures\\Skyboxes\\NiagaraFalls2\\negx.jpg", // Left
		ASSET_PATH"\\Textures\\Skyboxes\\NiagaraFalls2\\posy.jpg", // Top
		ASSET_PATH"\\Textures\\Skyboxes\\NiagaraFalls2\\negy.jpg", // Bottom
		ASSET_PATH"\\Textures\\Skyboxes\\NiagaraFalls2\\posz.jpg", // Front
		ASSET_PATH"\\Textures\\Skyboxes\\NiagaraFalls2\\negz.jpg", // Back
	};

	std::vector<Vertex> _allVertices;
	std::vector<uint32_t> _allIndices;
	std::vector<SurfaceMetaInfo> _allSurfaceMetaInfo;
	std::vector<VkDrawIndexedIndirectCommand> _allOpaqueDrawCommands;
	std::vector<VkDrawIndexedIndirectCommand> _allTransparentDrawCommands;
	//std::vector<VkDescriptorImageInfo> _allMaterialsInfo;
	std::vector<std::shared_ptr<GLTFMaterial>> _allMaterials;
	std::vector<ImageMetaInfo> _allImageMetaInfo;
	std::vector<Light> _allLights;
	std::vector<glm::mat4> _allFinalBoneTransforms;
	std::vector<glm::mat4> _allInvBindPoseTransforms;

	MultiImageHandle _allMaterialImages;

	ResourceHandle _surfaceMetaInfoBuffer;
	ResourceHandle _vertexBuffer;
	ResourceHandle _indexBuffer;
	ResourceHandle _opaqueDrawCommandBuffer;
	ResourceHandle _opaqueDrawCountBuffer;
	ResourceHandle _transparentDrawCommandBuffer;
	ResourceHandle _transparentDrawCountBuffer;
	ResourceHandle _imageMetaInfoBuffer;
	ResourceHandle _gpuTriangleCountBuffer;
	ResourceHandle _cpuTriangleCountBuffer;
	ResourceHandle _lightsBuffer;
	ResourceHandle _dirLightViewProjBuffer;
	ResourceHandle _indirLightViewProjBuffer;
	ResourceHandle _inverseIndirLightViewProjBuffer;
	ResourceHandle _kernelBuffer;
	ResourceHandle _boneTransformBuffer;
	ResourceHandle _invBindPoseTransformBuffer;

	RenderGraph _renderGraph;

	VkDeviceAddress _vertexBufferAddress;

	float _cameraExposure;

	bool _bIsLinkedToUnreal = false;

	uint32_t _totalDrawCounter = 0;
	uint32_t _opaqueDrawCounter = 0;
	uint32_t _transparentDrawCounter = 0;
	void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

	void InitShadowMapData();
	void InitReflectiveShadowData();

private:
	
	void InitVulkan();
	void InitSwapchain();
	void InitCommands();
	void InitSyncStructures();
	void InitDescriptors();
	void InitRenderGraph();

	void InitImgui();
	void InitDefaultData();
	void InitBindlessData();
	void InitGBufferData();
	void InitSkyboxData();
	void InitAmbientOcclusionData();
	void InitBlurData();
	void InitTemporalAAData();
	void InitIndirectLightingData();
	void InitIndirectLightingAccumulationComputeData();
	void InitRenderGraphData();

	void CreateSwapchain(uint32_t width, uint32_t height);
	void DestroySwapchain();
	void DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void ResizeSwapchain();
	
	void CreateFrustumPlanes(const glm::mat4& viewProj, const glm::mat4& view);
	std::vector<glm::mat4> CreateShadowCascades(const glm::mat4& cameraView, float cascadeWidth, const std::vector<glm::vec2>& cascadeNearFarPlanes);
};
