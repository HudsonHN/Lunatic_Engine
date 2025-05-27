//> includes
#include "vk_engine.h"

#include "VkBootstrap.h"

#include <chrono>
#include <thread>
#include <random>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <glm/gtx/transform.hpp>

#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_debug.h"

#define VMA_DEBUG_MEMORY_NAME 0
#define MIPMAP_SHADOWS 0

bool useValidationLayers = true;
LunaticEngine* loadedEngine = nullptr;

float Halton(uint32_t i, uint32_t b);

LunaticEngine& LunaticEngine::Get() { return *loadedEngine; }
void LunaticEngine::Init()
{
    // Only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    _window = SDL_CreateWindow(
        "Lunatic Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    InitVulkan();

    // Initialize memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    _mainCamera.velocity = glm::vec3(0.0f);
    _mainCamera.position = glm::vec3(0.0f, 0.0f, 0.0f);
    _mainCamera.pitch = 0.0f;
    _mainCamera.yaw = 0.0f;
    _cameraExposure = 1.0f;
    _cascadeExtentIndex = 2;

    _perfStats = {};

    _atrousFilterConstants = {};
    _atrousFilterConstants.phiColor = 50.0f;
    _atrousFilterConstants.phiPosition = 2.5f;
    _atrousFilterConstants.phiNormal = 64.0f;

    _lastTime = std::chrono::system_clock::now();

    auto initStart = std::chrono::system_clock::now();

    InitDescriptors();
    InitRenderGraph();
    InitSwapchain();
    InitCommands();
    InitSyncStructures();
    InitDefaultData();
    InitBindlessData();
    InitSkyboxData();
    InitGBufferData();
    InitShadowMapData();
    InitReflectiveShadowData();
    InitIndirectLightingData();
    InitIndirectLightingAccumulationComputeData();
    InitAmbientOcclusionData();
    InitBlurData();
    InitTemporalAAData();
    InitRenderGraphData();
    InitImgui();

    _ambientScale = 0.05f;
    _sceneData = {};
    _sceneData.sunlightColor = glm::vec4(1.f);
    _sceneData.sunlightColor.a = 15.0f;
    _sceneData.sunlightDirection = glm::vec4(0.0f, -1.0f, 0.5, 1.f);
    _sceneData.ambientColor = glm::vec4(1.0f);
    _sceneData.numLights = 0;
    _sceneData.renderScale = _renderScale;
    
    if (!_bIsLinkedToUnreal)
    {
        CreateLight(glm::vec3{ 1.0f }, glm::vec3{ 1.0f }, 1.0f, 0.0f, LightType::Point);

        LoadMesh(ASSET_PATH"\\Models\\Sponza\\Sponza.gltf", glm::mat4{ 1.0f });
        UploadMeshes();
    }
    
    auto initEnd = std::chrono::system_clock::now();
    _perfStats.initializationTime = std::chrono::duration_cast<std::chrono::microseconds>(initEnd - initStart).count() / 1000.0f;;

    // Everything went fine
    _isInitialized = true;
}

void LunaticEngine::LoadMesh(const std::string& path, const glm::mat4& worldTransform)
{
    auto file = LoadGltf(this, path);
    assert(file.has_value());

    _loadedScenes[path] = *file;
    _loadedScenes[path]->_worldTransform = worldTransform;
}

void LunaticEngine::CreateLight(glm::vec3 position, glm::vec3 color, float intensity, float radius, LightType type)
{
    Light light
    {
        .position = position,
        .color = color,
        .intensity = intensity,
        .radius = radius,
        .type = type,
    };

    _allLights.push_back(light);
    _sceneData.numLights++;
}

void LunaticEngine::Cleanup()
{
    if (_isInitialized) 
    {
        // Wait for GPU to finish
        vkDeviceWaitIdle(_device);
        
        _loadedScenes.clear();


        _mainDeletionQueue.Flush();

        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
            // Destroying a command pool destroys all command buffers allocated from it
            vkDestroyCommandPool(_device, _frames[i].commandPool, nullptr);

            // Destroying sync objects
            vkDestroyFence(_device, _frames[i].renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i].swapchainSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i].renderSemaphore, nullptr);

            _frames[i].deletionQueue.Flush();
        }

        _renderGraph.Cleanup();

        DestroySwapchain();

#if VMA_DEBUG_MEMORY_NAME
        fmt::println("All remaining buffers:");
        for (auto& pair : _allocatedBuffers)
        {
            fmt::println("{}", pair.first);
        }
        fmt::println("All remaining images:");
        for (auto& pair : _allocatedImages)
        {
            fmt::println("{}", pair.first);
        }
        char* statsString = nullptr;
        vmaBuildStatsString(_allocator, &statsString, VK_TRUE);
        printf("%s\n", statsString);
        vmaFreeStatsString(_allocator, statsString);
#endif
        vmaDestroyAllocator(_allocator);

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);

        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);
        SDL_DestroyWindow(_window);

    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void LunaticEngine::Run()
{
    SDL_Event e;
    bool shouldQuit = false;

    _lastTime = std::chrono::system_clock::now();
    // main loop
    while (!shouldQuit) 
    {
        _deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - _lastTime).count() / 1000.0f;
        _perfStats.frameTime = _deltaTime;
        _lastTime = std::chrono::system_clock::now();
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) 
        {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
            {
                shouldQuit = true;
            }
            
            if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_ESCAPE) 
                { 
                    shouldQuit = true; 
                }
            }

            if (e.type == SDL_MOUSEBUTTONUP)
            {
                if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    SDL_WarpMouseInWindow(_window, _windowExtent.width / 2, _windowExtent.height / 2);
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    SDL_WarpMouseInWindow(_window, _windowExtent.width / 2, _windowExtent.height / 2);
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
            }

            if (e.type == SDL_WINDOWEVENT) 
            {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) 
                {
                    _stopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) 
                {
                    _stopRendering = false;
                }
            }

            if (SDL_GetRelativeMouseMode() == SDL_TRUE)
            {
                _mainCamera.ProcessSDLEvent(e, _deltaTime);
            }
            else
            {
                // Send SDL event to imgui for handling
                ImGui_ImplSDL2_ProcessEvent(&e);
            }
        }

        // do not draw if we are minimized
        if (_stopRendering)
        {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (_resizeRequested)
        {
            ResizeSwapchain();
        }

        RunImGui();

        Draw();
    }
}

void LunaticEngine::RunImGui()
{
    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(_window);
    ImGui::NewFrame();

    ImGui::Begin("Stats");
    ImGui::Text("Initialization Time %f s", _perfStats.initializationTime / 1000.0f);
    ImGui::Text("Frame Time %f ms", _perfStats.frameTime);
    ImGui::Text("Draw Time %f ms", _perfStats.meshDrawTime);
    ImGui::Text("Scene Update Time %f ms", _perfStats.sceneUpdateTime);
    ImGui::Text("GPU Data Transfer Time %f ms", _perfStats.gpuDataTransferTime);
    ImGui::Text("Triangles %i", _perfStats.triangleCount);
    ImGui::Text("Draws %i", _perfStats.drawcallCount);
    if (ImGui::CollapsingHeader("Camera"))
    {
        ImGui::Text("Position X: %f, Y: %f, Z: %f", _mainCamera.position.x, _mainCamera.position.y, _mainCamera.position.z);
        ImGui::Text("Pitch: %f, Yaw: %f", _mainCamera.pitch, _mainCamera.yaw);
    }
    ImGui::End();

    ImGui::Begin("Settings");
    if (ImGui::CollapsingHeader("Render"))
    {
        ImGui::DragFloat("Scale", &_renderScale, 0.25f, 0.25f, 1.0f);
        ImGui::DragFloat("Exposure", &_cameraExposure, 0.1f, -4.0f, 4.0f);
        ImGui::DragFloat("Ambient Scale", &_ambientScale, 0.1f, 0.0f, 1.0f);
    }
    if (ImGui::CollapsingHeader("Shadows"))
    {
        ImGui::Checkbox("Directional Shadows", &_bDrawDirectionalShadows);
        ImGui::Checkbox("Reflective Shadows", &_bDrawReflectiveDirectionalShadows);
        //ImGui::SliderInt("Shadow Quality", &_cascadeExtentIndex, 0, 2);
        ImGui::Checkbox("Apply Bias", &_bBiasShadowMaps);
        if (_bBiasShadowMaps)
        {
            ImGui::SliderFloat("Min Bias Factor", &_minBiasFactor, 0.0001f, 0.01f);
            ImGui::SliderFloat("Max Bias Factor", &_maxBiasFactor, 0.01f, 0.05f);
        }
        if (_bDrawReflectiveDirectionalShadows && ImGui::CollapsingHeader("Reflective Shadows"))
        {
            //ImGui::Checkbox("Use Compute Shader", &_bUseIndirectLightingCompute);
            ImGui::Checkbox("Temporal Accumulation", &_bApplyIndirectAccumulation);
            ImGui::DragFloat("Intensity", &_reflectiveIntensity, 1.0f, 0.0f, 100.0f);
            ImGui::SliderInt("Indirect Light Samples", &_numReflectiveShadowSamples, 1, 1024);
            ImGui::SliderFloat("Sample Radius", &_sampleRadius, 0.02f, 1.0f);
            ImGui::Checkbox("Apply Atrous Filter", &_bApplyAtrousFilter);
            if (_bApplyAtrousFilter && ImGui::CollapsingHeader("Atrous Filtering"))
            {
                ImGui::SliderInt("Sample Count", &_atrousFilterNumSamples, 0, 10);
                ImGui::DragFloat("Color Influence", &_atrousFilterConstants.phiColor, 0.0f, 0.0f, 10.0f);
                ImGui::DragFloat("Normal Influence", &_atrousFilterConstants.phiNormal, 0.0f, 0.0f, 10.0f);
                ImGui::DragFloat("Position Influence", &_atrousFilterConstants.phiPosition, 0.0f, 0.0f, 10.0f);
            }
        }
    }
    if (ImGui::CollapsingHeader("Post Processing"))
    {
        ImGui::Checkbox("Tonemapping", &_bApplyTonemap);
        ImGui::Checkbox("TAA", &_bApplyTAA);
        if (_bApplyTAA && ImGui::CollapsingHeader("Anti-Aliasing"))
        {
            ImGui::DragFloat("TAA Jitter Scale", &_jitterScale, 0.1f, 0.0f, 3.0f);
            ImGui::DragFloat2("TAA DeltaTime Max Weight", (float*)&_deltaTimeMinMaxWeight, 0.1f, 0.1f, 1.0f);
        }
        ImGui::Checkbox("SSAO", &_bApplySSAO);
        if (_bApplySSAO && ImGui::CollapsingHeader("Ambient Occlusion"))
        {
            ImGui::Checkbox("Blur", &_bApplySSAOBlur);
            ImGui::DragFloat("Radius", &_ssaoConstants.radius, 0.25f, 0.0f, 10.0f);
            ImGui::SliderFloat("Bias", &_ssaoConstants.bias, 0.0f, 0.25f);
            ImGui::SliderFloat("Power", &_ssaoConstants.power, 0.1f, 10.0f);
            ImGui::SliderFloat("Blur Amount", &_ssaoBlurAmount, 0.0f, 10.0f);
        }
    }
    ImGui::End();

    ImGui::Begin("Lights");
    ImGui::SliderFloat3("Sun Direction", (float*)&_sceneData.sunlightDirection, -1.0f, 1.0f);
    ImGui::SliderFloat3("Sun Color", (float*)&_sceneData.sunlightColor, 0.0f, 1.0f);
    ImGui::SliderFloat("Sun Power", (float*)&_sceneData.sunlightColor.w, 0.0f, 100.0f);
    for (size_t i = 0; i < _allLights.size(); i++)
    {
        ImGui::Separator();
        std::string text = std::format("Position {}", i);
        ImGui::DragFloat3(text.c_str(), (float*)&_allLights[i].position, 0.1f);
        text = std::format("RGB Color {}", i);
        ImGui::SliderFloat3(text.c_str(), (float*)&_allLights[i].color, 0.0f, 1.0f);
        text = std::format("Intensity {}", i);
        ImGui::SliderFloat(text.c_str(), (float*)&_allLights[i].intensity, 0.0f, 100.0f);
        text = std::format("Radius {}", i);
        ImGui::SliderFloat(text.c_str(), (float*)&_allLights[i].radius, 0.0f, 20.0f);
    }
    ImGui::End();

    // Make imgui calculate internal draw structures
    ImGui::Render();
}

void LunaticEngine::TestRun()
{
    SDL_Event e;

    _lastTime = std::chrono::system_clock::now();
    // main loop
    while (!shouldQuit) 
    {
        _deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - _lastTime).count() / 1000.0f;
        _perfStats.frameTime = _deltaTime;
        _lastTime = std::chrono::system_clock::now();
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) 
        {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
            {
                shouldQuit = true;
            }
            
            if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_ESCAPE) 
                { 
                    shouldQuit = true;
                }
            }

            if (e.type == SDL_MOUSEBUTTONUP)
            {
                if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    SDL_WarpMouseInWindow(_window, _windowExtent.width / 2, _windowExtent.height / 2);
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
            }
            if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    SDL_WarpMouseInWindow(_window, _windowExtent.width / 2, _windowExtent.height / 2);
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
            }

            if (e.type == SDL_WINDOWEVENT) 
            {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) 
                {
                    _stopRendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) 
                {
                    _stopRendering = false;
                }
            }

            if (SDL_GetRelativeMouseMode() == SDL_TRUE)
            {
                _mainCamera.ProcessSDLEvent(e, _deltaTime);
            }
            else
            {
                // Send SDL event to imgui for handling
                ImGui_ImplSDL2_ProcessEvent(&e);
            }
        }

        // do not draw if we are minimized
        if (_stopRendering)
        {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if (_resizeRequested)
        {
            ResizeSwapchain();
        }

        RunImGui();

        Draw();
    }
}

void LunaticEngine::UERun()
{
    SDL_Event e;

    _deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - _lastTime).count() / 1000.0f;
    _perfStats.frameTime = _deltaTime;
    // Handle events on queue
    while (SDL_PollEvent(&e) != 0)
    {

        if (e.type == SDL_KEYDOWN)
        {

        }

        if (e.type == SDL_MOUSEBUTTONUP)
        {
            if (e.button.button == SDL_BUTTON_RIGHT)
            {
                SDL_WarpMouseInWindow(_window, _windowExtent.width / 2, _windowExtent.height / 2);
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN)
        {
            if (e.button.button == SDL_BUTTON_RIGHT)
            {
                SDL_WarpMouseInWindow(_window, _windowExtent.width / 2, _windowExtent.height / 2);
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
        }

        if (e.type == SDL_WINDOWEVENT)
        {
            if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
            {
                _stopRendering = true;
            }
            if (e.window.event == SDL_WINDOWEVENT_RESTORED)
            {
                _stopRendering = false;
            }
        }

        if (SDL_GetRelativeMouseMode() == SDL_TRUE)
        {
            _mainCamera.ProcessSDLEvent(e, _deltaTime);
        }
        else
        {
            // Send SDL event to imgui for handling
            ImGui_ImplSDL2_ProcessEvent(&e);
        }
    }

    // do not draw if we are minimized
    if (_stopRendering)
    {
        // throttle the speed to avoid the endless spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return;
    }
    if (_resizeRequested)
    {
        ResizeSwapchain();
    }

    RunImGui();
    Draw();
    _lastTime = std::chrono::system_clock::now();
}

void LunaticEngine::InitVulkan()
{
    vkb::InstanceBuilder builder;

    // Make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Lunatic Engine")
        .request_validation_layers(useValidationLayers)
        //.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // Grab the instance 
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    vkdebug::LoadDebugLabelFunctions(_instance);

    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    // Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features.dynamicRendering = true;
    features.synchronization2 = true;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;
    features12.drawIndirectCount = true;
    features12.runtimeDescriptorArray = true;
    features12.descriptorBindingVariableDescriptorCount = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingStorageBufferUpdateAfterBind = true;
    features12.descriptorBindingUniformBufferUpdateAfterBind = true;
    features12.descriptorBindingSampledImageUpdateAfterBind = true;
    features12.descriptorBindingStorageImageUpdateAfterBind = true;
    features12.shaderSampledImageArrayNonUniformIndexing = true;

    features.pNext = &features12;
    // Vulkan 1.1 features
    //VkPhysicalDeviceVulkan11Features features11{};
    //features11.shaderDrawParameters = true;

    // Use vkbootstrap to select a gpu. 
    // We want a GPU that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features)
        //.set_required_features_12(features12)
        //.set_required_features_11(features11)
        .set_surface(_surface)
        .select()
        .value();


    //create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{ physicalDevice };

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void LunaticEngine::InitSwapchain()
{
    CreateSwapchain(_windowExtent.width, _windowExtent.height);

    // Draw image size will match the window
    VkExtent3D drawImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };
    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

    _drawImage = _renderGraph.CreateImage("GPU_draw image", drawImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages);
    _renderGraph.AddDirtyResource(&_drawImage);

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    _depthImage = _renderGraph.CreateImage("GPU_depth image", drawImageExtent, VK_FORMAT_D32_SFLOAT_S8_UINT, depthImageUsages);
    _renderGraph.AddDirtyResource(&_depthImage);

    _intermediateImage = _renderGraph.CreateImage("GPU_intermediate image", drawImageExtent, VK_FORMAT_R8G8B8A8_UNORM, drawImageUsages);
    _renderGraph.AddDirtyResource(&_intermediateImage);
}

void LunaticEngine::InitCommands()
{
    // Create a command pool for commands submitted to the graphics queue.
    // We also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++) {

        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i].commandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i].commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i].mainCommandBuffer));
    }

    {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));
        // Allocate cmd buffer for immediate submits
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

        _mainDeletionQueue.PushFunction([=]() {
            vkDestroyCommandPool(_device, _immCommandPool, nullptr);
        });
    }
}

void LunaticEngine::InitSyncStructures()
{
    // Create syncronization structures
    // One fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to synchronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i].renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].renderSemaphore));
    }

    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    _mainDeletionQueue.PushFunction([=]() {
        vkDestroyFence(_device, _immFence, nullptr);
    });
}

void LunaticEngine::CreateSwapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU,_device,_surface };

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        //use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    _swapchainExtent = vkbSwapchain.extent;
    // Store swapchain and its related images
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void LunaticEngine::DestroySwapchain()
{
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    // Destroy swapchain resources
    for (int i = 0; i < _swapchainImageViews.size(); i++)
    {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
}

void LunaticEngine::InitDescriptors()
{
    {
        // Create a descriptor pool that will hold 10 sets with 1 image each
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = 
        {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
        };
        _globalDescriptorAllocator.Init(_device, 20, sizes);

        _mainDeletionQueue.PushFunction([&]() {
            _globalDescriptorAllocator.DestroyPools(_device);
        });
    }

    {
        // Create a descriptor pool for our bindless model
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes =
        {
            // To hold our texture indices, vertex buffer indices, etc.
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            // To hold our storage images (e.g. G-buffer, compute output, etc.)
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
            // To hold our bindless textures + samplers
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES + MAX_LIGHTS },
        };
        _bindlessDescriptorAllocator.Init(_device, 2000, sizes, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
        _mainDeletionQueue.PushFunction([&]() {
            _bindlessDescriptorAllocator.DestroyPools(_device);
            });
    }

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        // Create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        _frames[i].frameDescriptors = DescriptorAllocatorGrowable{};
        _frames[i].frameDescriptors.Init(_device, 1000, frameSizes);

        _mainDeletionQueue.PushFunction([&, i]() {
            _frames[i].frameDescriptors.DestroyPools(_device);
        });
    }
}

void LunaticEngine::InitRenderGraph()
{
    _renderGraph.engine = this;
    _renderGraph.bindlessAllocator = &_bindlessDescriptorAllocator;
    _renderGraph.globalAllocator = &_globalDescriptorAllocator;

    _allMaterialImages = _renderGraph.CreateMultiImage();
}

void LunaticEngine::InitRenderGraphData()
{
    _renderGraph.Setup();
}

void LunaticEngine::InitDefaultData() {
    // 3 default textures, white, grey, black. 1 pixel each
    uint32_t white = 0xFFFFFFFF;
    _whiteImage = _renderGraph.CreateImage("white", (void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = 0xFFAAAAAA;
    _greyImage = _renderGraph.CreateImage("grey", (void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = 0xFF000000;
    _blackImage = _renderGraph.CreateImage("black", (void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    // Checkerboard image
    // Alpha, Blue, Green, Red
    uint32_t magenta = 0xFFFF00FF;
    std::array<uint32_t, 16 * 16> pixels; // For 16x16 checkerboard texture
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    _errorCheckerboardImage = _renderGraph.CreateImage("error", pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo samplerCreateInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(_device, &samplerCreateInfo, nullptr, &_defaultSamplerNearest);

    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_device, &samplerCreateInfo, nullptr, &_defaultSamplerLinear);

    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(_device, &samplerCreateInfo, nullptr, &_defaultSamplerRepeat);

    _mainDeletionQueue.PushFunction([=]()
        {
            vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
            vkDestroySampler(_device, _defaultSamplerLinear, nullptr);
            vkDestroySampler(_device, _defaultSamplerRepeat, nullptr);
        });

    AllocatedImage& whiteImage = _renderGraph.GetImage(_whiteImage);

    MaterialResources materialResources;
    // Default the material textures
    materialResources.colorImage = whiteImage;
    materialResources.colorSampler = _defaultSamplerLinear;
    materialResources.metalRoughImage = whiteImage;
    materialResources.metalRoughSampler = _defaultSamplerLinear;
    materialResources.normalImage = whiteImage;
    materialResources.normalSampler = _defaultSamplerLinear;
    materialResources.constants.colorFactors = glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
    materialResources.constants.metalRoughFactors = glm::vec4{ 1.0f, 0.5f, 0.0f, 0.0f };

    WriteMaterial(_device, materialResources, _globalDescriptorAllocator);
}

void LunaticEngine::InitBindlessData()
{
    static constexpr uint32_t thirtyTwoMegabytes = 33554432;
    _vertexBuffer = _renderGraph.CreateBuffer(thirtyTwoMegabytes,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global vertex"
    );
    _renderGraph.AddDirtyResource(&_vertexBuffer);

    VkBufferDeviceAddressInfo vertexAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = _renderGraph.GetBuffer(_vertexBuffer).buffer
    };
    _vertexBufferAddress = vkGetBufferDeviceAddress(_device, &vertexAddressInfo);

    _indexBuffer = _renderGraph.CreateBuffer(thirtyTwoMegabytes,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global index"
    );
    _renderGraph.AddDirtyResource(&_indexBuffer);

    _surfaceMetaInfoBuffer = _renderGraph.CreateBuffer(256,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global meta"
    );
    _renderGraph.AddDirtyResource(&_surfaceMetaInfoBuffer);

    _opaqueDrawCommandBuffer = _renderGraph.CreateBuffer(MAX_DRAWS * sizeof(VkDrawIndexedIndirectCommand),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global opaque draw cmd"
    );
    _renderGraph.AddDirtyResource(&_opaqueDrawCommandBuffer);

    _opaqueDrawCountBuffer = _renderGraph.CreateBuffer(16,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global opaque draw count"
    );
    _renderGraph.AddDirtyResource(&_opaqueDrawCountBuffer);

    _transparentDrawCommandBuffer = _renderGraph.CreateBuffer(MAX_DRAWS * sizeof(VkDrawIndexedIndirectCommand),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global transparent draw cmd"
    );
    _renderGraph.AddDirtyResource(&_transparentDrawCommandBuffer);

    _transparentDrawCountBuffer = _renderGraph.CreateBuffer(16,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global transparent draw count"
    );
    _renderGraph.AddDirtyResource(&_transparentDrawCountBuffer);

    _imageMetaInfoBuffer = _renderGraph.CreateBuffer(thirtyTwoMegabytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global textures info"
    );
    _renderGraph.AddDirtyResource(&_imageMetaInfoBuffer);

    _gpuTriangleCountBuffer = _renderGraph.CreateBuffer(sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "perf stats"
    );
    _renderGraph.GetBuffer(_gpuTriangleCountBuffer).usedSize = sizeof(uint32_t);
    _renderGraph.AddDirtyResource(&_gpuTriangleCountBuffer);

    _cpuTriangleCountBuffer = _renderGraph.CreateBuffer(sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY, "cpu triangle buffer",
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    _renderGraph.AddDirtyResource(&_cpuTriangleCountBuffer);

    _lightsBuffer = _renderGraph.CreateBuffer(MAX_LIGHTS * sizeof(Light),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global lights"
    );
    _renderGraph.AddDirtyResource(&_lightsBuffer);

    _dirLightViewProjBuffer = _renderGraph.CreateBuffer(NUM_CASCADES * sizeof(glm::mat4),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global dir light view proj"
    );
    _renderGraph.AddDirtyResource(&_dirLightViewProjBuffer);

    _indirLightViewProjBuffer = _renderGraph.CreateBuffer(NUM_CASCADES * sizeof(glm::mat4),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global indir light view proj"
    );
    _renderGraph.AddDirtyResource(&_indirLightViewProjBuffer);

    _inverseIndirLightViewProjBuffer = _renderGraph.CreateBuffer(NUM_CASCADES * sizeof(glm::mat4),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "global inverse indir light view proj"
    );
    _renderGraph.AddDirtyResource(&_inverseIndirLightViewProjBuffer);
}

void LunaticEngine::InitGBufferData()
{
    VkExtent3D drawImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
    _positionColor = _renderGraph.CreateImage("GPU_position deferred", drawImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages);
    _normalColor = _renderGraph.CreateImage("GPU_normal deferred", drawImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages);
    _albedoSpecColor = _renderGraph.CreateImage("GPU_albedospecular deferred", drawImageExtent, VK_FORMAT_R8G8B8A8_UNORM, drawImageUsages);
    _metalRoughnessColor = _renderGraph.CreateImage("GPU_metalroughness deferred", drawImageExtent, VK_FORMAT_R8G8B8A8_UNORM, drawImageUsages);
    _velocityImage = _renderGraph.CreateImage("GPU_velocity", drawImageExtent, VK_FORMAT_R16G16_SFLOAT, drawImageUsages | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    _renderGraph.AddDirtyResource(&_positionColor);
    _renderGraph.AddDirtyResource(&_normalColor);
    _renderGraph.AddDirtyResource(&_albedoSpecColor);
    _renderGraph.AddDirtyResource(&_metalRoughnessColor);
}

void LunaticEngine::InitShadowMapData()
{
    if (_shadowMapImages.index != -1)
    {
        _renderGraph.DestroyMultiImage(_shadowMapImages);
    }
    _shadowMapImages = _renderGraph.CreateMultiImage();

    std::vector<VkDescriptorImageInfo>& imageInfos = _renderGraph.GetMultiImage(_shadowMapImages);
    for (int i = 0; i < NUM_CASCADES; i++)
    {
        VkExtent3D drawImageExtent = {
            _cascadeExtents[_cascadeExtentIndex].width,
            _cascadeExtents[_cascadeExtentIndex].height,
            1
        };
#if MIPMAP_SHADOWS
        _cascadeDepth[i] = _renderGraph.CreateImage("GPU_shadow depth image", drawImageExtent,
            VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            true);
#else
        _cascadeDepth[i] = _renderGraph.CreateImage("GPU_shadow depth image", drawImageExtent,
            VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            false);
#endif
        _renderGraph.AddDirtyResource(&_cascadeDepth[i]);
    
        AllocatedImage& cascadeDepth = _renderGraph.GetImage(_cascadeDepth[i]);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = _defaultSamplerLinear;
        imageInfo.imageView = cascadeDepth.imageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // What the shader expects the image layout to be when it's read

        imageInfos.push_back(imageInfo);
    }

    _renderGraph.AddDirtyMultiImage(&_shadowMapImages);
}

void LunaticEngine::InitIndirectLightingData()
{
    VkExtent3D lightingPassExtent =
    {
        _windowExtent.width,
        _windowExtent.height,
        1
    };
    _indirectLightColor = _renderGraph.CreateImage("GPU_reflect shadow light pass", lightingPassExtent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        false);
    _renderGraph.AddDirtyResource(&_indirectLightColor);
}

void LunaticEngine::InitIndirectLightingAccumulationComputeData()
{
    VkExtent3D lightingPassExtent =
    {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    _prevIndirectLightColor = _renderGraph.CreateImage("GPU_reflect shadow light pass prev", lightingPassExtent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        false);
    _historyIndirectLightColor = _renderGraph.CreateImage("GPU_reflect shadow light pass history", lightingPassExtent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        false);

    _renderGraph.AddDirtyResource(&_prevIndirectLightColor);
    _renderGraph.AddDirtyResource(&_historyIndirectLightColor);

    AllocatedImage& prevIndirectLightColor = _renderGraph.GetImage(_prevIndirectLightColor);

    ImmediateSubmit([=](VkCommandBuffer cmd)
        {
            vkutil::TransitionImage(cmd, prevIndirectLightColor.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
}

void LunaticEngine::InitReflectiveShadowData()
{
    _reflectiveShadowMapImages = _renderGraph.CreateMultiImage();

    std::vector<VkDescriptorImageInfo>& imageInfos = _renderGraph.GetMultiImage(_reflectiveShadowMapImages);
    for (int i = 0; i < NUM_CASCADES * 3; i += 3)
    {
        VkExtent3D reflectImageExtent = {
            _cascadeExtents[_cascadeExtentIndex].width / 4,
            _cascadeExtents[_cascadeExtentIndex].height / 4,
            1
        };

        _cascadeReflectDepth[i / 3] = _renderGraph.CreateImage("GPU_reflect shadow depth image", reflectImageExtent,
            VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT| VK_IMAGE_USAGE_SAMPLED_BIT,
            false);
        _cascadePosition[i / 3] = _renderGraph.CreateImage("GPU_reflect shadow position image", reflectImageExtent,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            false);
        _cascadeNormal[i / 3] = _renderGraph.CreateImage("GPU_reflect shadow normal image", reflectImageExtent,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            false);
        _cascadeFlux[i / 3] = _renderGraph.CreateImage("GPU_reflect shadow flux image", reflectImageExtent,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            false);

        _renderGraph.AddDirtyResource(&_cascadeReflectDepth[i / 3]);
        _renderGraph.AddDirtyResource(&_cascadePosition[i / 3]);
        _renderGraph.AddDirtyResource(&_cascadeNormal[i / 3]);
        _renderGraph.AddDirtyResource(&_cascadeFlux[i / 3]);

        VkDescriptorImageInfo positionImageInfo{};
        positionImageInfo.sampler = _defaultSamplerNearest;
        positionImageInfo.imageView = _renderGraph.GetImage(_cascadePosition[i / 3]).imageView;
        positionImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo normalImageInfo{};
        normalImageInfo.sampler = _defaultSamplerNearest;
        normalImageInfo.imageView = _renderGraph.GetImage(_cascadeNormal[i / 3]).imageView;
        normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo fluxImageInfo{};
        fluxImageInfo.sampler = _defaultSamplerLinear;
        fluxImageInfo.imageView = _renderGraph.GetImage(_cascadeFlux[i / 3]).imageView;
        fluxImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageInfos.push_back(positionImageInfo);
        imageInfos.push_back(normalImageInfo);
        imageInfos.push_back(fluxImageInfo);
    }

    _renderGraph.AddDirtyMultiImage(&_reflectiveShadowMapImages);
}

void LunaticEngine::InitSkyboxData()
{
    //std::string cubeMapPaths[6] =
    //{
    //    ASSET_PATH"\\Textures\\NiagaraFalls2\\posx.jpg", // Right
    //    ASSET_PATH"\\Textures\\NiagaraFalls2\\negx.jpg", // Left
    //    ASSET_PATH"\\Textures\\NiagaraFalls2\\posy.jpg", // Top
    //    ASSET_PATH"\\Textures\\NiagaraFalls2\\negy.jpg", // Bottom
    //    ASSET_PATH"\\Textures\\NiagaraFalls2\\posz.jpg", // Front
    //    ASSET_PATH"\\Textures\\NiagaraFalls2\\negz.jpg", // Back
    //};
    _cubeMap = _renderGraph.CreateCubemap(_cubeMapPaths);

    _renderGraph.AddDirtyResource(&_cubeMap);
}

void LunaticEngine::InitTemporalAAData()
{
    for (uint32_t i = 0; i < _jitterCount; i++)
    {
        float haltonX = 2.0f * Halton(i + 1, 2) - 1.0f;
        float haltonY = 2.0f * Halton(i + 1, 3) - 1.0f;
        _haltonJitterOffsets.push_back(glm::vec2(haltonX, haltonY));
    }

    VkExtent3D extents =
    {
        .width = _windowExtent.width,
        .height = _windowExtent.height,
        .depth = 1
    };
    _historyImage = _renderGraph.CreateImage("GPU_history TAA", extents, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    _prevVelocityImage = _renderGraph.CreateImage("GPU_previous velocity", extents, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    _prevFrameImage = _renderGraph.CreateImage("GPU_previous frame", extents, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    _renderGraph.AddDirtyResource(&_historyImage);
    _renderGraph.AddDirtyResource(&_velocityImage);
    _renderGraph.AddDirtyResource(&_prevVelocityImage);
    _renderGraph.AddDirtyResource(&_prevFrameImage);

    AllocatedImage& prevFrameImage = _renderGraph.GetImage(_prevFrameImage);
    AllocatedImage& prevVelocityImage = _renderGraph.GetImage(_prevVelocityImage);

    ImmediateSubmit([=](VkCommandBuffer cmd) {
            vkutil::TransitionImage(cmd, prevFrameImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            vkutil::TransitionImage(cmd, prevVelocityImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        });
}

void LunaticEngine::InitAmbientOcclusionData()
{
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;

    _ssaoConstants.bias = 0.05f;
    _ssaoConstants.radius = 1.0f;
    _ssaoConstants.kernelSize = 16;
    _ssaoConstants.power = 3.0f;
    _ssaoBlurAmount = 2.0f;

    for (uint32_t i = 0; i < _ssaoConstants.kernelSize; ++i)
    {
        glm::vec3 sample(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);

        float scale = static_cast<float>(i) / static_cast<float>(_ssaoConstants.kernelSize);
        scale = std::lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        _ssaoKernel.push_back(sample);
    }

    uint32_t kernelSize = sizeof(glm::vec3) * static_cast<uint32_t>(_ssaoKernel.size());

    AllocatedBuffer stagingBuffer = CreateBuffer(kernelSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, "kernel staging");
    _kernelBuffer = _renderGraph.CreateBuffer(kernelSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, "kernel buffer");
    _renderGraph.AddDirtyResource(&_kernelBuffer);

    AllocatedBuffer& kernelBuffer = _renderGraph.GetBuffer(_kernelBuffer);

    void* data;
    vmaMapMemory(_allocator, stagingBuffer.allocation, &data);
    memcpy(data, _ssaoKernel.data(), kernelSize);
    vmaUnmapMemory(_allocator, stagingBuffer.allocation);

    ImmediateSubmit([&](VkCommandBuffer cmd)
    {
        VkBufferCopy copyInfo{};
        copyInfo.srcOffset = 0;
        copyInfo.dstOffset = 0;
        copyInfo.size = kernelSize;

        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, kernelBuffer.buffer, 1, &copyInfo);
        kernelBuffer.usedSize = kernelSize;
    });

    DestroyBuffer(stagingBuffer);

    std::vector<glm::vec3> ssaoNoise;
    for (uint32_t i = 0; i < 64; i++)
    {
        glm::vec3 noise(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            0.0f);
        ssaoNoise.push_back(noise);
    }

    VkExtent3D screenExtent
    {
        .width = _windowExtent.width,
        .height = _windowExtent.height,
        .depth = 1
    };

    _noiseAOTexture = _renderGraph.CreateImage("SSAO noise", ssaoNoise.data(), VkExtent3D{ 8, 8, 1 },
        VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT);
    _ssaoColor = _renderGraph.CreateImage("GPU_ssao color", screenExtent, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    _renderGraph.AddDirtyResource(&_noiseAOTexture);
    _renderGraph.AddDirtyResource(&_ssaoColor);
}

void LunaticEngine::InitBlurData()
{
    /*VkExtent3D extents =
    {
        .width = _windowExtent.width,
        .height = _windowExtent.height,
        .depth = 1
    };

    _blurredImage = CreateImage("GPU_blurred image", extents, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    _mainDeletionQueue.PushFunction([=]() {
        DestroyImage(_blurredImage);
    });*/
}

void LunaticEngine::InitImgui()
{
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(_window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _chosenGPU;
    init_info.Device = _device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.ColorAttachmentFormat = _swapchainImageFormat;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

    // execute a gpu command to upload imgui font textures
    ImmediateSubmit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

    // clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    // add the destroy the imgui created structures
    _mainDeletionQueue.PushFunction([=]() {
        vkDestroyDescriptorPool(_device, imguiPool, nullptr);
        ImGui_ImplSDL2_Shutdown();
        ImGui_ImplVulkan_Shutdown();
    });
}

void LunaticEngine::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

    // Submit cmd buffer to the queue and execute it
    // renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void LunaticEngine::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr, nullptr);
    
    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void LunaticEngine::Draw()
{
    UpdateScene();
    UploadRuntimeInfo();

    auto start = std::chrono::system_clock::now();
    // Wait until the GPU has finished rendering the last frame
    // Timeout of 1 second
    VK_CHECK(vkWaitForFences(_device, 1, &GetCurrentFrame().renderFence, true, 1000000000));

    _perfStats.drawcallCount = 0;

    GetCurrentFrame().deletionQueue.Flush();
    GetCurrentFrame().frameDescriptors.ClearPools(_device);

    VK_CHECK(vkResetFences(_device, 1, &GetCurrentFrame().renderFence));

    // Request image from the swapchain
    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR)
    {
        _resizeRequested = true;
        return;
    }

    // Naming it cmd for shorter writing
    VkCommandBuffer cmd = GetCurrentFrame().mainCommandBuffer;

    // Now that we are sure that the commands finished executing, we can safely
    // Reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // Begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    _renderGraph.Run(cmd, swapchainImageIndex);

    // Draw imgui into the swapchain image
    DrawImgui(cmd, _swapchainImageViews[swapchainImageIndex]);

    // Set swapchain image layout to present so we can draw it
    vkutil::TransitionImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // Finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Prepare the submission to the queue. 
    // We will signal the _renderSemaphore, to signal that rendering has finished
    // Then we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    VkCommandBufferSubmitInfo primaryCmdInfo = vkinit::command_buffer_submit_info(cmd);

    VkCommandBufferSubmitInfo cmdInfos[] =
    {
        primaryCmdInfo
    };

    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().swapchainSemaphore);

    VkSubmitInfo2 submitInfo = vkinit::submit_info(cmdInfos, &signalInfo, &waitInfo, 1);
    // Submit command buffer to the queue and execute it.
    // renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, GetCurrentFrame().renderFence));

    // Prepare present
    // This will put the image we just rendered to into the visible window.
    // We want to wait on the _renderSemaphore for that, 
    // As its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        _resizeRequested = true;
    }

    AllocatedBuffer& cpuTriangleCountBuffer = _renderGraph.GetBuffer(_cpuTriangleCountBuffer);

    uint32_t* data;
    vmaMapMemory(_allocator, cpuTriangleCountBuffer.allocation, (void**)&data);
    _perfStats.triangleCount = *data;
    vmaUnmapMemory(_allocator, cpuTriangleCountBuffer.allocation);

    // Increase the number of frames drawn
    _frameNumber++;
    auto end = std::chrono::system_clock::now();
    float deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f;
    _perfStats.meshDrawTime = deltaTime;
}

void LunaticEngine::ResizeSwapchain()
{
    vkDeviceWaitIdle(_device);

    DestroySwapchain();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;

    CreateSwapchain(_windowExtent.width, _windowExtent.height);

    _resizeRequested = false;
}

AllocatedBuffer LunaticEngine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, std::string name)
{
    // Allocate buffer
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer newBuffer;
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, 
            &newBuffer.allocation, &newBuffer.info));
#if VMA_DEBUG_MEMORY_NAME
    vmaSetAllocationName(_allocator, newBuffer.allocation, name.c_str());
#endif
    newBuffer.name = name;
    newBuffer.usedSize = 0;

    //fmt::println("Creating buffer {}", name);
    /*if (_allocatedBuffers.contains(name))
    {
        fmt::println("Buffer {} has already been allocated before!", name);
    }
    _allocatedBuffers[name] = newBuffer;*/

    return newBuffer;
}

AllocatedBuffer LunaticEngine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, std::string name, VkMemoryPropertyFlags propertyFlags)
{
    // Allocate buffer
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    vmaAllocInfo.requiredFlags = propertyFlags;

    AllocatedBuffer newBuffer;
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer,
        &newBuffer.allocation, &newBuffer.info));
#if VMA_DEBUG_MEMORY_NAME
    vmaSetAllocationName(_allocator, newBuffer.allocation, name.c_str());
#endif
    newBuffer.name = name;
    newBuffer.usedSize = 0;
    //fmt::println("Creating buffer {}", name);

    /*if (_allocatedBuffers.contains(name))
    {
        fmt::println("Buffer {} has already been allocated before!", name);
    }
    _allocatedBuffers[name] = newBuffer;*/
    return newBuffer;
}

void LunaticEngine::DestroyBuffer(const AllocatedBuffer& buffer)
{
    //fmt::println("Destroying buffer {}", buffer.name);
    //_allocatedBuffers.erase(buffer.name);
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void LunaticEngine::ResizeBufferGPU(AllocatedBuffer& currentBuffer, uint32_t prevSize, uint32_t newSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage)
{
    std::string newName = currentBuffer.name + "_resized";

    AllocatedBuffer tempBuffer = CreateBuffer(newSize * 2, usageFlags, memoryUsage, newName);
    if (prevSize > 0)
    {
        ImmediateSubmit([&](VkCommandBuffer cmd)
        {
            VkBufferCopy bufferCopy{ 0 };
            bufferCopy.dstOffset = 0;
            bufferCopy.srcOffset = 0;
            bufferCopy.size = prevSize;

            vkCmdCopyBuffer(cmd, currentBuffer.buffer, tempBuffer.buffer, 1, &bufferCopy);
        });
    }
    DestroyBuffer(currentBuffer);
    currentBuffer = tempBuffer;

}

void LunaticEngine::ResizeBufferGPU(AllocatedBuffer& currentBuffer, uint32_t prevSize, uint32_t newSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage, VkMemoryPropertyFlags propertyFlags)
{
    std::string newName = currentBuffer.name + "_resized";

    AllocatedBuffer tempBuffer = CreateBuffer(newSize * 2, usageFlags, memoryUsage, newName, propertyFlags);    
    if (prevSize > 0)
    {
        ImmediateSubmit([&](VkCommandBuffer cmd)
        {
            VkBufferCopy bufferCopy{ 0 };
            bufferCopy.dstOffset = 0;
            bufferCopy.srcOffset = 0;
            bufferCopy.size = prevSize;

            vkCmdCopyBuffer(cmd, currentBuffer.buffer, tempBuffer.buffer, 1, &bufferCopy);
        });
    }
    DestroyBuffer(currentBuffer);
    currentBuffer = tempBuffer;
}

void LunaticEngine::SetupMesh(MeshNode* meshNode)
{
    for (GeoSurface& surface : meshNode->_mesh->surfaces)
    {
        meshNode->_indices[surface.name] = _totalDrawCounter;

        SurfaceMetaInfo metaInfo;
        metaInfo.surfaceIndex = _totalDrawCounter; // THIS MUST CORRESPOND WITH THE DRAW COMMAND FIRST INSTANCE
        metaInfo.materialIndex = surface.materialIndex;
        metaInfo.bounds = surface.bounds;
        //metaInfo.bIsTransparent = surface.material->_data.matPassType == MaterialPass::Transparent;
        //metaInfo.drawIndex = surface.material->_data.matPassType == MaterialPass::Transparent ? transparentDrawCounter : opaqueDrawCounter;
        metaInfo.bIsTransparent = surface.materialType == MaterialPass::Transparent;
        metaInfo.drawIndex = surface.materialType == MaterialPass::Transparent ? _transparentDrawCounter : _opaqueDrawCounter;

        _allSurfaceMetaInfo.push_back(metaInfo);

        VkDrawIndexedIndirectCommand drawCommand{};
        drawCommand.indexCount = surface.indexCount; // How many indices to iterate through in this draw
        drawCommand.instanceCount = 1;
        drawCommand.firstIndex = surface.indexOffset; // firstIndex is indexOffset, aka where the index for this draw starts in the global index buffer
        drawCommand.vertexOffset = 0; // Leave at 0 since we're using a global vertex buffer, firstIndex will handle the offset between different meshes
        drawCommand.firstInstance = _totalDrawCounter; // This is what determines gl_InstanceIndex, so use it to index into the SurfaceMetaInfo buffer

        // _perfStats.triangleCount += surface.indexCount / 3;
        
        // Set up logic here to split between opaque and transparent draws later
        //switch (surface.material->_data.matPassType)
        switch (surface.materialType)
        {
            case MaterialPass::MainColor:
            {
                _allOpaqueDrawCommands.push_back(drawCommand);
                _opaqueDrawCounter++;
                break;
            }
            case MaterialPass::Transparent:
            {
                _allTransparentDrawCommands.push_back(drawCommand);
                _transparentDrawCounter++;
                break;
            }
            default:
            {
                _allOpaqueDrawCommands.push_back(drawCommand);
                _opaqueDrawCounter++;
                break;
            }
        }
        _totalDrawCounter++;
    }
}

void LunaticEngine::UpdateMesh(uint32_t index, const glm::mat4& worldTransform)
{
    _allSurfaceMetaInfo[index].worldTransform = worldTransform;
}

void LunaticEngine::UploadMeshes()
{
    const uint32_t vertexBufferSize = static_cast<uint32_t>(_allVertices.size()) * static_cast<uint32_t>(sizeof(Vertex));
    const uint32_t indexBufferSize = static_cast<uint32_t>(_allIndices.size()) * static_cast<uint32_t>(sizeof(uint32_t));
    const uint32_t drawOpaqueIndirectCommandBufferSize = static_cast<uint32_t>(_allOpaqueDrawCommands.size()) * static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand));
    const uint32_t drawTransparentIndirectCommandBufferSize = static_cast<uint32_t>(_allTransparentDrawCommands.size()) * static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand));
    const uint32_t imageMetaInfoBufferSize = static_cast<uint32_t>(_allImageMetaInfo.size()) * static_cast<uint32_t>(sizeof(ImageMetaInfo));
    
    AllocatedBuffer& vertexBuffer = _renderGraph.GetBuffer(_vertexBuffer);
    AllocatedBuffer& indexBuffer = _renderGraph.GetBuffer(_indexBuffer);
    AllocatedBuffer& opaqueDrawCommandBuffer = _renderGraph.GetBuffer(_opaqueDrawCommandBuffer);
    AllocatedBuffer& opaqueDrawCountBuffer = _renderGraph.GetBuffer(_opaqueDrawCountBuffer);
    AllocatedBuffer& transparentDrawCommandBuffer = _renderGraph.GetBuffer(_transparentDrawCommandBuffer);
    AllocatedBuffer& transparentDrawCountBuffer = _renderGraph.GetBuffer(_transparentDrawCountBuffer);
    AllocatedBuffer& imageMetaInfoBuffer = _renderGraph.GetBuffer(_imageMetaInfoBuffer);

    const uint32_t newGlobalVertexBufferSize = vertexBufferSize + vertexBuffer.usedSize;
    const uint32_t newGlobalIndexBufferSize = indexBufferSize + vertexBuffer.usedSize;
    const uint32_t newGlobalOpaqueDrawCommandBufferSize = drawOpaqueIndirectCommandBufferSize + opaqueDrawCommandBuffer.usedSize;
    const uint32_t newGlobalOpaqueDrawCountBufferSize = static_cast<uint32_t>(sizeof(uint32_t)) + opaqueDrawCountBuffer.usedSize;
    const uint32_t newGlobalTransparentDrawCommandBufferSize = drawTransparentIndirectCommandBufferSize + transparentDrawCommandBuffer.usedSize;
    const uint32_t newGlobalTransparentDrawCountBufferSize = static_cast<uint32_t>(sizeof(uint32_t)) + transparentDrawCountBuffer.usedSize;
    const uint32_t newGlobalImageMetaInfoBufferSize = imageMetaInfoBufferSize + imageMetaInfoBuffer.usedSize;

    uint32_t maxGlobalVertexBufferSize = static_cast<uint32_t>(vertexBuffer.info.size);
    uint32_t maxGlobalIndexBufferSize = static_cast<uint32_t>(indexBuffer.info.size);
    uint32_t maxGlobalOpaqueDrawCommandBufferSize = static_cast<uint32_t>(opaqueDrawCommandBuffer.info.size);
    uint32_t maxGlobalOpaqueDrawCountBufferSize = static_cast<uint32_t>(opaqueDrawCountBuffer.info.size);
    uint32_t maxGlobalTransparentDrawCommandBufferSize = static_cast<uint32_t>(opaqueDrawCommandBuffer.info.size);
    uint32_t maxGlobalTransparentDrawCountBufferSize = static_cast<uint32_t>(opaqueDrawCountBuffer.info.size);
    uint32_t maxGlobalImageMetaInfoBufferSize = static_cast<uint32_t>(imageMetaInfoBuffer.info.size);
    
    // Destroy and recreate our global buffers with a bigger size if they're not big enough for the newly added data
    while (newGlobalVertexBufferSize > maxGlobalVertexBufferSize)
    {
        fmt::println("Resizing vertex buffer");
        ResizeBufferGPU(vertexBuffer, vertexBuffer.usedSize, newGlobalVertexBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);
        VkBufferDeviceAddressInfo vertexAddressInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = vertexBuffer.buffer
        };
        _renderGraph.AddDirtyResource(&_vertexBuffer);
        _vertexBufferAddress = vkGetBufferDeviceAddress(_device, &vertexAddressInfo);

        maxGlobalVertexBufferSize = static_cast<uint32_t>(vertexBuffer.info.size);
    }
    while (newGlobalIndexBufferSize > maxGlobalIndexBufferSize)
    {
        fmt::println("Resizing index buffer");
        ResizeBufferGPU(indexBuffer, indexBuffer.usedSize, newGlobalIndexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        maxGlobalIndexBufferSize = static_cast<uint32_t>(indexBuffer.info.size);
    }
    while (newGlobalOpaqueDrawCommandBufferSize > maxGlobalOpaqueDrawCommandBufferSize)
    {
        fmt::println("Resizing opaque draw command buffer");
        ResizeBufferGPU(opaqueDrawCommandBuffer, opaqueDrawCommandBuffer.usedSize, newGlobalOpaqueDrawCommandBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        maxGlobalOpaqueDrawCommandBufferSize = static_cast<uint32_t>(opaqueDrawCommandBuffer.info.size);
    }
    while (newGlobalOpaqueDrawCountBufferSize > maxGlobalOpaqueDrawCountBufferSize)
    {
        fmt::println("Resizing opaque draw count buffer");
        ResizeBufferGPU(opaqueDrawCountBuffer, opaqueDrawCountBuffer.usedSize, newGlobalOpaqueDrawCountBufferSize,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        maxGlobalOpaqueDrawCountBufferSize = static_cast<uint32_t>(opaqueDrawCountBuffer.info.size);
    }
    while (newGlobalTransparentDrawCommandBufferSize > maxGlobalTransparentDrawCommandBufferSize)
    {
        fmt::println("Resizing transparent draw command buffer");
        ResizeBufferGPU(transparentDrawCommandBuffer, transparentDrawCommandBuffer.usedSize, newGlobalTransparentDrawCommandBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        maxGlobalTransparentDrawCommandBufferSize = static_cast<uint32_t>(transparentDrawCommandBuffer.info.size);
    }
    while (newGlobalTransparentDrawCountBufferSize > maxGlobalTransparentDrawCountBufferSize)
    {
        fmt::println("Resizing transparent draw count buffer");
        ResizeBufferGPU(transparentDrawCountBuffer, transparentDrawCountBuffer.usedSize, newGlobalTransparentDrawCountBufferSize,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        maxGlobalTransparentDrawCountBufferSize = static_cast<uint32_t>(transparentDrawCountBuffer.info.size);
    }
    while (newGlobalImageMetaInfoBufferSize > maxGlobalImageMetaInfoBufferSize)
    {
        fmt::println("Resizing image meta info buffer");
        ResizeBufferGPU(imageMetaInfoBuffer, imageMetaInfoBuffer.usedSize, newGlobalImageMetaInfoBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        _renderGraph.AddDirtyResource(&_imageMetaInfoBuffer);
        maxGlobalImageMetaInfoBufferSize = static_cast<uint32_t>(imageMetaInfoBuffer.info.size);
    }

    uint32_t opaqueDrawCount = static_cast<uint32_t>(_allOpaqueDrawCommands.size());
    uint32_t transparentDrawCount = static_cast<uint32_t>(_allTransparentDrawCommands.size());

    AllocatedBuffer staging = CreateBuffer(
                            vertexBufferSize + indexBufferSize + drawOpaqueIndirectCommandBufferSize + sizeof(uint32_t) + drawTransparentIndirectCommandBufferSize + sizeof(uint32_t) + imageMetaInfoBufferSize,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                            VMA_MEMORY_USAGE_CPU_ONLY, "staging mesh");

    void* _data = nullptr;
    VK_CHECK(vmaMapMemory(_allocator, staging.allocation, &_data));

    // Copy vertex buffer
    memcpy(_data, 
        _allVertices.data(), vertexBufferSize);
    // Copy index buffer
    memcpy((char*)_data + vertexBufferSize,
        _allIndices.data(), indexBufferSize);
    // Copy draw opaque indirect commands
    memcpy((char*)_data + vertexBufferSize + indexBufferSize, 
        _allOpaqueDrawCommands.data(), drawOpaqueIndirectCommandBufferSize);
    // Copy draw opaque count
    memcpy((char*)_data + vertexBufferSize + indexBufferSize + drawOpaqueIndirectCommandBufferSize, 
        &opaqueDrawCount, sizeof(uint32_t));
    // Copy draw transparent indirect commands
    memcpy((char*)_data + vertexBufferSize + indexBufferSize + drawOpaqueIndirectCommandBufferSize + sizeof(uint32_t),
        _allTransparentDrawCommands.data(), drawTransparentIndirectCommandBufferSize);
    // Copy draw transparent count
    memcpy((char*)_data + vertexBufferSize + indexBufferSize + drawOpaqueIndirectCommandBufferSize + sizeof(uint32_t) + drawTransparentIndirectCommandBufferSize,
        &transparentDrawCount, sizeof(uint32_t));
    // Copy image meta info
    memcpy((char*)_data + vertexBufferSize + indexBufferSize + drawOpaqueIndirectCommandBufferSize + sizeof(uint32_t) + drawTransparentIndirectCommandBufferSize + sizeof(uint32_t),
        _allImageMetaInfo.data(), imageMetaInfoBufferSize);
    
    vmaUnmapMemory(_allocator, staging.allocation);

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        if (vertexBufferSize > 0)
        {
            vkCmdCopyBuffer(cmd, staging.buffer, vertexBuffer.buffer, 1, &vertexCopy);
        }

        vertexBuffer.usedSize = vertexBufferSize;

        VkBufferCopy indexCopy{};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        if (indexBufferSize)
        {
            vkCmdCopyBuffer(cmd, staging.buffer, indexBuffer.buffer, 1, &indexCopy);
        }

        indexBuffer.usedSize = indexBufferSize;

        VkBufferCopy opaqueDrawInfoCopy{};
        opaqueDrawInfoCopy.dstOffset = 0;
        opaqueDrawInfoCopy.srcOffset = vertexBufferSize + indexBufferSize;
        opaqueDrawInfoCopy.size = drawOpaqueIndirectCommandBufferSize;

        if (opaqueDrawInfoCopy.size > 0)
        {
            vkCmdCopyBuffer(cmd, staging.buffer, opaqueDrawCommandBuffer.buffer, 1, &opaqueDrawInfoCopy);
        }

        opaqueDrawCommandBuffer.usedSize = drawOpaqueIndirectCommandBufferSize;

        VkBufferCopy opaqueDrawCountCopy{};
        opaqueDrawCountCopy.dstOffset = 0;
        opaqueDrawCountCopy.srcOffset = vertexBufferSize + indexBufferSize + drawOpaqueIndirectCommandBufferSize;
        opaqueDrawCountCopy.size = sizeof(uint32_t);

        vkCmdCopyBuffer(cmd, staging.buffer, opaqueDrawCountBuffer.buffer, 1, &opaqueDrawCountCopy);

        opaqueDrawCountBuffer.usedSize = sizeof(uint32_t);

        VkBufferCopy transparentDrawInfoCopy{};
        transparentDrawInfoCopy.dstOffset = 0;
        transparentDrawInfoCopy.srcOffset = vertexBufferSize + indexBufferSize + drawOpaqueIndirectCommandBufferSize + sizeof(uint32_t);
        transparentDrawInfoCopy.size = drawTransparentIndirectCommandBufferSize;

        if (transparentDrawInfoCopy.size > 0)
        {
            vkCmdCopyBuffer(cmd, staging.buffer, transparentDrawCommandBuffer.buffer, 1, &transparentDrawInfoCopy);
        }

        transparentDrawCommandBuffer.usedSize = drawTransparentIndirectCommandBufferSize;

        VkBufferCopy transparentDrawCountCopy{};
        transparentDrawCountCopy.dstOffset = 0;
        transparentDrawCountCopy.srcOffset = vertexBufferSize + indexBufferSize + drawOpaqueIndirectCommandBufferSize + sizeof(uint32_t) + drawTransparentIndirectCommandBufferSize;
        transparentDrawCountCopy.size = sizeof(uint32_t);

        vkCmdCopyBuffer(cmd, staging.buffer, transparentDrawCountBuffer.buffer, 1, &transparentDrawCountCopy);

        transparentDrawCountBuffer.usedSize = sizeof(uint32_t);

        VkBufferCopy imageMetaInfoCopy{};
        imageMetaInfoCopy.dstOffset = 0;
        imageMetaInfoCopy.srcOffset = vertexBufferSize + indexBufferSize + drawOpaqueIndirectCommandBufferSize +
                                            sizeof(uint32_t) + drawTransparentIndirectCommandBufferSize + sizeof(uint32_t);
        imageMetaInfoCopy.size = imageMetaInfoBufferSize;

        if (imageMetaInfoCopy.size > 0)
        {
            vkCmdCopyBuffer(cmd, staging.buffer, imageMetaInfoBuffer.buffer, 1, &imageMetaInfoCopy);
        }

        imageMetaInfoBuffer.usedSize = imageMetaInfoBufferSize;
    });

    DestroyBuffer(staging);
}

void LunaticEngine::UploadRuntimeInfo()
{
    auto start = std::chrono::system_clock::now();

    AllocatedBuffer& surfaceMetaInfoBuffer = _renderGraph.GetBuffer(_surfaceMetaInfoBuffer);
    AllocatedBuffer& lightsBuffer = _renderGraph.GetBuffer(_lightsBuffer);
    AllocatedBuffer& dirLightViewProjBuffer = _renderGraph.GetBuffer(_dirLightViewProjBuffer);
    AllocatedBuffer& indirLightViewProjBuffer = _renderGraph.GetBuffer(_indirLightViewProjBuffer);
    AllocatedBuffer& inverseIndirLightViewProjBuffer = _renderGraph.GetBuffer(_inverseIndirLightViewProjBuffer);

    const uint32_t surfaceMetaInfoBufferSize = static_cast<uint32_t>(_allSurfaceMetaInfo.size()) * static_cast<uint32_t>(sizeof(SurfaceMetaInfo));
    const uint32_t newSurfaceMetaInfoBufferSize = surfaceMetaInfoBufferSize + surfaceMetaInfoBuffer.usedSize;

    const uint32_t lightsBufferSize = static_cast<uint32_t>(_allLights.size()) * static_cast<uint32_t>(sizeof(Light));
    const uint32_t newLightsBufferSize = lightsBufferSize + lightsBuffer.usedSize;

    const uint32_t dirLightViewProjBufferSize = static_cast<uint32_t>(NUM_CASCADES) * static_cast<uint32_t>(sizeof(glm::mat4));
    const uint32_t newDirLightViewProjBufferSize = dirLightViewProjBufferSize + dirLightViewProjBuffer.usedSize;

    const uint32_t indirLightViewProjBufferSize = static_cast<uint32_t>(NUM_CASCADES) * static_cast<uint32_t>(sizeof(glm::mat4));
    const uint32_t newIndirLightViewProjBufferSize = indirLightViewProjBufferSize + indirLightViewProjBuffer.usedSize;

    const uint32_t inverseIndirLightViewProjBufferSize = static_cast<uint32_t>(NUM_CASCADES) * static_cast<uint32_t>(sizeof(glm::mat4));
    const uint32_t newInverseIndirLightViewProjBufferSize = inverseIndirLightViewProjBufferSize + inverseIndirLightViewProjBuffer.usedSize;

    uint32_t maxSurfaceMetaInfoBufferSize = static_cast<uint32_t>(surfaceMetaInfoBuffer.info.size);
    uint32_t maxLightsBufferSize = static_cast<uint32_t>(lightsBuffer.info.size);
    uint32_t maxDirLightViewProjBufferSize = static_cast<uint32_t>(dirLightViewProjBuffer.info.size);
    uint32_t maxIndirLightViewProjBufferSize = static_cast<uint32_t>(indirLightViewProjBuffer.info.size);
    uint32_t maxInverseIndirLightViewProjBufferSize = static_cast<uint32_t>(inverseIndirLightViewProjBuffer.info.size);

    // Destroy and recreate our global buffers with a bigger size if they're not big enough for the newly added data
    while (newSurfaceMetaInfoBufferSize > maxSurfaceMetaInfoBufferSize)
    {
        fmt::println("Resizing meta info buffer");
        ResizeBufferGPU(surfaceMetaInfoBuffer, surfaceMetaInfoBuffer.usedSize, newSurfaceMetaInfoBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        _renderGraph.AddDirtyResource(&_surfaceMetaInfoBuffer);
        maxSurfaceMetaInfoBufferSize = static_cast<uint32_t>(surfaceMetaInfoBuffer.info.size);
    }
    while (newLightsBufferSize > maxLightsBufferSize)
    {
        fmt::println("Resizing lights buffer");
        ResizeBufferGPU(lightsBuffer, lightsBuffer.usedSize, newLightsBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);
        
        _renderGraph.AddDirtyResource(&_lightsBuffer);
        maxLightsBufferSize = static_cast<uint32_t>(lightsBuffer.info.size);
    }
    while (newDirLightViewProjBufferSize > maxDirLightViewProjBufferSize)
    {
        fmt::println("Resizing dir lights view proj buffer");
        ResizeBufferGPU(dirLightViewProjBuffer, dirLightViewProjBuffer.usedSize, newDirLightViewProjBufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        _renderGraph.AddDirtyResource(&_dirLightViewProjBuffer);
        maxDirLightViewProjBufferSize = static_cast<uint32_t>(dirLightViewProjBuffer.info.size);
    }
    while (newIndirLightViewProjBufferSize > maxIndirLightViewProjBufferSize)
    {
        fmt::println("Resizing indir lights view proj buffer");
        ResizeBufferGPU(indirLightViewProjBuffer, indirLightViewProjBuffer.usedSize, newIndirLightViewProjBufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        _renderGraph.AddDirtyResource(&_indirLightViewProjBuffer);
        maxIndirLightViewProjBufferSize = static_cast<uint32_t>(indirLightViewProjBuffer.info.size);
    }
    while (newInverseIndirLightViewProjBufferSize > maxInverseIndirLightViewProjBufferSize)
    {
        fmt::println("Resizing inverse indir lights view proj buffer");
        ResizeBufferGPU(inverseIndirLightViewProjBuffer, inverseIndirLightViewProjBuffer.usedSize, newInverseIndirLightViewProjBufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY);

        _renderGraph.AddDirtyResource(&_inverseIndirLightViewProjBuffer);
        maxInverseIndirLightViewProjBufferSize = static_cast<uint32_t>(inverseIndirLightViewProjBuffer.info.size);
    }
    
    AllocatedBuffer staging = CreateBuffer(
        surfaceMetaInfoBufferSize + lightsBufferSize + dirLightViewProjBufferSize + indirLightViewProjBufferSize + inverseIndirLightViewProjBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY, "staging meta");
    // Copy mesh info into GPU data through the CPU's data mapping into the GPU
    void* _data = nullptr;
    VK_CHECK(vmaMapMemory(_allocator, staging.allocation, &_data));
    memcpy(_data, _allSurfaceMetaInfo.data(), surfaceMetaInfoBufferSize);
    memcpy((char*)_data + surfaceMetaInfoBufferSize, _allLights.data(), lightsBufferSize);
    memcpy((char*)_data + surfaceMetaInfoBufferSize + lightsBufferSize, _dirLightViewProj.data(), dirLightViewProjBufferSize);
    memcpy((char*)_data + surfaceMetaInfoBufferSize + lightsBufferSize + dirLightViewProjBufferSize, _indirLightViewProj.data(), indirLightViewProjBufferSize);
    memcpy((char*)_data + surfaceMetaInfoBufferSize + lightsBufferSize + dirLightViewProjBufferSize + indirLightViewProjBufferSize, _inverseIndirLightViewProj.data(), inverseIndirLightViewProjBufferSize);
    vmaUnmapMemory(_allocator, staging.allocation);

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy metaInfoCopy{};
        metaInfoCopy.dstOffset = 0;
        metaInfoCopy.srcOffset = 0;
        metaInfoCopy.size = surfaceMetaInfoBufferSize;
        vkCmdCopyBuffer(cmd, staging.buffer, surfaceMetaInfoBuffer.buffer, 1, &metaInfoCopy);

        surfaceMetaInfoBuffer.usedSize = surfaceMetaInfoBufferSize;

        VkBufferCopy lightsInfoCopy{};
        lightsInfoCopy.dstOffset = 0;
        lightsInfoCopy.srcOffset = surfaceMetaInfoBufferSize;
        lightsInfoCopy.size = lightsBufferSize;
        vkCmdCopyBuffer(cmd, staging.buffer, lightsBuffer.buffer, 1, &lightsInfoCopy);

        lightsBuffer.usedSize = lightsBufferSize;

        VkBufferCopy viewProjInfoCopy{};
        viewProjInfoCopy.dstOffset = 0;
        viewProjInfoCopy.srcOffset = surfaceMetaInfoBufferSize + lightsBufferSize;
        viewProjInfoCopy.size = dirLightViewProjBufferSize;
        vkCmdCopyBuffer(cmd, staging.buffer, dirLightViewProjBuffer.buffer, 1, &viewProjInfoCopy);

        dirLightViewProjBuffer.usedSize = dirLightViewProjBufferSize;

        VkBufferCopy indirViewProjInfoCopy{};
        indirViewProjInfoCopy.dstOffset = 0;
        indirViewProjInfoCopy.srcOffset = surfaceMetaInfoBufferSize + lightsBufferSize + dirLightViewProjBufferSize;
        indirViewProjInfoCopy.size = indirLightViewProjBufferSize;
        vkCmdCopyBuffer(cmd, staging.buffer, indirLightViewProjBuffer.buffer, 1, &indirViewProjInfoCopy);

        indirLightViewProjBuffer.usedSize = indirLightViewProjBufferSize;

        VkBufferCopy inverseIndirViewProjInfoCopy{};
        inverseIndirViewProjInfoCopy.dstOffset = 0;
        inverseIndirViewProjInfoCopy.srcOffset = surfaceMetaInfoBufferSize + lightsBufferSize + dirLightViewProjBufferSize + indirLightViewProjBufferSize;
        inverseIndirViewProjInfoCopy.size = inverseIndirLightViewProjBufferSize;
        vkCmdCopyBuffer(cmd, staging.buffer, inverseIndirLightViewProjBuffer.buffer, 1, &inverseIndirViewProjInfoCopy);

        inverseIndirLightViewProjBuffer.usedSize = inverseIndirLightViewProjBufferSize;
    });

    DestroyBuffer(staging);

    auto end = std::chrono::system_clock::now();
    float deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f;

    _perfStats.gpuDataTransferTime = deltaTime;
}

AllocatedImage LunaticEngine::LoadCubemap(std::string paths[6])
{
    int width = 0; // Only one of each needed since a cube is assumed to be equal on all sides
    int height = 0;
    int channel = 0;

    void* pixels[6];

    AllocatedImage& errorCheckerboardImage = _renderGraph.GetImage(_errorCheckerboardImage);

    //bool loadSuccess = true;
    for (uint32_t i = 0; i < 6; i++)
    {
        pixels[i] = vkutil::LoadImageDataFile(paths[i], width, height, channel);
        if (!pixels[i])
        {
            return errorCheckerboardImage;
        }
    }

    const VkDeviceSize imageSize = width * height * 4 * 6;
    const VkDeviceSize layerSize = imageSize / 6;

    AllocatedBuffer uploadBuffer = CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "staging cube map");

    void* stagingData;
    vmaMapMemory(_allocator, uploadBuffer.allocation, &stagingData);
    for (uint32_t i = 0; i < 6; i++)
    {
        memcpy((char*)stagingData + (layerSize * i), pixels[i], layerSize);
        vkutil::FreeLoadedImage(pixels[i]);
    }
    //memcpy(stagingData, pixels, imageSize);
    vmaUnmapMemory(_allocator, uploadBuffer.allocation);

    VkExtent3D extent
    {
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .depth = 1
    };

    AllocatedImage cubeImage = CreateImage("cubemap", extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false, VK_IMAGE_VIEW_TYPE_CUBE, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);

    ImmediateSubmit([&](VkCommandBuffer cmd)
    {
        vkutil::TransitionImage(cmd, cubeImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 6;
        copyRegion.imageExtent = extent;

        // Copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, cubeImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &copyRegion);
#if 0
        if (mipmapped)
        {
            vkutil::GenerateMipmaps(cmd, newImage.image, VkExtent2D{ newImage.imageExtent.width, newImage.imageExtent.height });
        }
        else
#endif
        {
            vkutil::TransitionImage(cmd, cubeImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    DestroyBuffer(uploadBuffer);

    return cubeImage;
}

AllocatedImage LunaticEngine::LoadImageFromFile(const char* file, VkFormat format, VkImageUsageFlags usageFlags, bool mipmapped, bool cubeMap)
{ 
    int texWidth, texHeight, texChannels;

    AllocatedImage& errorCheckerboardImage = _renderGraph.GetImage(_errorCheckerboardImage);

    void* pixels = vkutil::LoadImageDataFile(file, texWidth, texHeight, texChannels);
    if (!pixels)
    {
        return errorCheckerboardImage;
    }
    
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkExtent3D extent{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};

    //the format R8G8B8A8 matches exactly with the pixels loaded from stb_image lib
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;

    VkImageCreateFlags createFlags = cubeMap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    VkImageViewType viewType = cubeMap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    uint32_t arrayLayers = cubeMap ? 6 : 1;

    AllocatedImage newImage = CreateImage(file, pixels, extent, imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT, true, viewType, createFlags, cubeMap);

    fmt::println("Texture loaded successfully {}", file);

    return newImage;
}

// Helper to allocate an image for the GPU only
AllocatedImage LunaticEngine::CreateImage(std::string name, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VkImageViewType viewType, VkImageCreateFlags createFlags, uint32_t arrayLayers)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo imgInfo = vkinit::image_create_info(format, usage, size, createFlags);
    if (mipmapped) 
    {
        imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }
    imgInfo.arrayLayers = arrayLayers;

    // Always allocate images on dedicated GPU memory
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create the image
    VK_CHECK(vmaCreateImage(_allocator, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));
#if VMA_DEBUG_MEMORY_NAME
    vmaSetAllocationName(_allocator, newImage.allocation, name.c_str());
#endif
    // If the format is a depth format, we will need to have it use the correct aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else if (format == VK_FORMAT_D32_SFLOAT)
    {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // Build a image-view for the image
    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag, viewType);
    view_info.subresourceRange.layerCount = arrayLayers;
    view_info.subresourceRange.levelCount = imgInfo.mipLevels;

    VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

    newImage.name = name;
    _allocatedImages[name] = newImage;

    return newImage;
}

// Initially allocate a buffer on CPU to transfer data over to an image that's allocated on GPU
AllocatedImage LunaticEngine::CreateImage(std::string name, void* _data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped, VkImageViewType viewType, VkImageCreateFlags createFlags, uint32_t arrayLayers)
{
    size_t dataSize = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadBuffer = CreateBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "staging image");

    memcpy(uploadBuffer.info.pMappedData, _data, dataSize);

    std::string newImageName = "GPU_" + name;
    AllocatedImage newImage = CreateImage(newImageName, size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped, viewType, createFlags, arrayLayers);

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        // Copy the buffer into the image
        vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &copyRegion);

        if (mipmapped)
        {
            vkutil::GenerateMipmaps(cmd, newImage.image, VkExtent2D{newImage.imageExtent.width, newImage.imageExtent.height});
        }
        else
        {
            vkutil::TransitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    DestroyBuffer(uploadBuffer);

    _allocatedImages[newImage.name] = newImage;

    return newImage;
}

void LunaticEngine::DestroyImage(const AllocatedImage& img)
{
    //fmt::println("Deleting image: {}", img.name);
    _allocatedImages.erase(img.name);
    vkDestroyImageView(_device, img.imageView, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
}

void LunaticEngine::CreateFrustumPlanes(const glm::mat4& viewProj, const glm::mat4& view)
{
    glm::vec4 planes[6];

    // Left plane
    planes[0] = viewProj[3] + viewProj[0];
 
    // Right plane
    planes[1] = viewProj[3] - viewProj[0];

    // Bottom plane
    planes[2] = viewProj[3] + viewProj[1];

    // Top plane
    planes[3] = viewProj[3] - viewProj[1];

    // Near plane
    planes[4] = viewProj[3] + viewProj[2];

    // Far plane
    planes[5] = viewProj[3] - viewProj[2];

    for (int i = 0; i < 6; i++)
    {
        planes[i] = glm::normalize(planes[i]);
        //_frustumCullConstants.frustumPlanes[i].normal = glm::vec3(planes[i]);
        //_frustumCullConstants.frustumPlanes[i].distance = planes[i].w;
    }
}

// Get world coordinates of the camera frustum based on NDC coords
// note that in Vulkan, NDC space is [-1,1] for x,y, but not z, which is [0,1] instead
std::vector<glm::vec4> GetFrustumCornersWorldSpace(const glm::mat4& viewProj) 
{
    glm::mat4 inv = glm::inverse(viewProj);

    std::vector<glm::vec4> frustumCorners;
    frustumCorners.reserve(8);

    constexpr glm::vec3 localCorners[8] =
    {
        glm::vec3(-1.0f,  1.0f,  0.0f),
        glm::vec3( 1.0f,  1.0f,  0.0f),
        glm::vec3( 1.0f, -1.0f,  0.0f),
        glm::vec3(-1.0f, -1.0f,  0.0f),
        glm::vec3(-1.0f,  1.0f,  1.0f),
        glm::vec3( 1.0f,  1.0f,  1.0f),
        glm::vec3( 1.0f, -1.0f,  1.0f),
        glm::vec3(-1.0f, -1.0f,  1.0f),
    };

    for (int i = 0; i < 8; i++)
    {
        glm::vec4 corner = inv * glm::vec4(localCorners[i], 1.0f);
        corner /= corner.w;
        frustumCorners.push_back(corner);
    }

    return frustumCorners;
}

std::vector<glm::mat4> LunaticEngine::CreateShadowCascades(const glm::mat4& cameraView, float cascadeWidth, const std::vector<glm::vec2>& cascadeNearFarPlanes)
{
    std::vector<glm::mat4> lightViewProj;
    lightViewProj.resize(NUM_CASCADES);

    glm::vec3 sunDir = glm::normalize(glm::vec3(_sceneData.sunlightDirection));

    glm::mat4 frustumCascades[NUM_CASCADES];
    for (int i = 0; i < NUM_CASCADES; i++)
    {
        frustumCascades[i] = glm::perspective(glm::radians(70.0f), (float)_windowExtent.width / (float)_windowExtent.height,
            cascadeNearFarPlanes[i][0], cascadeNearFarPlanes[i][1]);
        frustumCascades[i][1][1] *= -1;
    }

    for (int i = 0; i < NUM_CASCADES; i++)
    {
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

        float upDotLight = std::abs(glm::dot(sunDir, up));
        if (upDotLight > 0.99f)
        {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        std::vector<glm::vec4> cameraFrustumCorners = GetFrustumCornersWorldSpace(frustumCascades[i] * cameraView);

        glm::vec4 cameraFrustumCenterWorldPos = glm::vec4(0.0f);
        for (glm::vec4& corner : cameraFrustumCorners)
        {
            cameraFrustumCenterWorldPos += corner;
        }
        cameraFrustumCenterWorldPos /= cameraFrustumCorners.size();

        float radius = 0.0f;
        for (int i = 0; i < (int)cameraFrustumCorners.size(); i++)
        {
            radius = std::max(radius, glm::length(cameraFrustumCorners[i] - cameraFrustumCenterWorldPos));
        }
        float texelsPerUnit = cascadeWidth / (radius * 2.0f);

        glm::mat4 scalar = glm::scale(glm::vec3(texelsPerUnit, texelsPerUnit, texelsPerUnit));

        glm::mat4 tempView = glm::lookAt
        (
            sunDir,
            glm::vec3(0.0f),
            up
        );
        tempView = tempView * scalar;
        glm::mat4 invView = glm::inverse(tempView);

        glm::vec3 frustumCenter = tempView * cameraFrustumCenterWorldPos;
        frustumCenter.x = std::floorf(frustumCenter.x);
        frustumCenter.y = std::floorf(frustumCenter.y);
        frustumCenter = invView * glm::vec4(frustumCenter, 1.0f);

        glm::vec3 newEye = frustumCenter + (sunDir * radius * 2.0f);
        glm::mat4 finalView = glm::lookAt
        (
            newEye,
            frustumCenter,
            up
        );

        glm::mat4 proj = glm::ortho(-radius, radius, -radius, radius, -radius * 6.0f, radius * 6.0f);

        lightViewProj[i] = proj * finalView;
    }
    return lightViewProj;
}

void LunaticEngine::UpdateScene()
{
    auto start = std::chrono::system_clock::now();

    AllocatedImage& drawImage = _renderGraph.GetImage(_drawImage);

    _drawExtent.width = static_cast<uint32_t>(std::min(_swapchainExtent.width, drawImage.imageExtent.width) * _renderScale);
    _drawExtent.height = static_cast<uint32_t>(std::min(_swapchainExtent.height, drawImage.imageExtent.height) * _renderScale);

    _mainCamera.Update(_deltaTime);
    
    _prevSceneData = _sceneData;

    glm::mat4 cameraView = _mainCamera.getViewMatrix();
    _sceneData.cameraPos = _mainCamera.position;
    _sceneData.view = cameraView;
    // camera projection
    _sceneData.proj = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    _sceneData.proj[1][1] *= -1;
    _sceneData.viewProj = _sceneData.proj * _sceneData.view;
    _sceneData.deltaTime = _deltaTime;

    //float jitterWeight = glm::clamp(1.0f / _deltaTime, _deltaTimeMinMaxWeight.x, _deltaTimeMinMaxWeight.y);
    _sceneData.jitterOffset = _haltonJitterOffsets[_frameNumber % _jitterCount];
    _sceneData.jitterOffset.x = (_sceneData.jitterOffset.x / static_cast<float>(_drawExtent.width)) * _jitterScale;
    _sceneData.jitterOffset.y = (_sceneData.jitterOffset.y / static_cast<float>(_drawExtent.height)) * _jitterScale;
    _sceneData.bApplyTAA = _bApplyTAA;
    _sceneData.renderScale = _renderScale;
    _sceneData.frameIndex = static_cast<uint32_t>(_frameNumber);

    _lightingConstants =
    {
        .bDrawShadowMaps = _bDrawDirectionalShadows ? 1 : 0,
        .bDrawReflectiveShadowMaps = _bDrawReflectiveDirectionalShadows ? 1 : 0,
        .numIndirectLightSamples = _numReflectiveShadowSamples,
        .bDrawSsao = _bApplySSAO ? 1 : 0,
        .sampleRadius = _sampleRadius,
        .reflectiveIntensity = _reflectiveIntensity,
        .ambientScale = _ambientScale,
        .minBiasFactor = _bBiasShadowMaps ? _minBiasFactor : 0.0f,
        .maxBiasFactor = _bBiasShadowMaps ? _maxBiasFactor : 0.0f,
    };

    _dirLightViewProj = CreateShadowCascades(cameraView, static_cast<float>(_cascadeExtents[_cascadeExtentIndex].width), _cascadeNearFarPlanes);
    _indirLightViewProj = CreateShadowCascades(cameraView, static_cast<float>(_cascadeExtents[_cascadeExtentIndex].width / 4), _indirectCascadeNearFarPlanes);
    _inverseIndirLightViewProj.clear();
    for (const glm::mat4& viewProj : _indirLightViewProj)
    {
        _inverseIndirLightViewProj.emplace_back(glm::inverse(viewProj));
    }

    _frustumCullConstants.viewProj = _sceneData.viewProj;

    for (auto& scene : _loadedScenes)
    {
        scene.second->Draw(scene.second->_worldTransform);
    }

    auto end = std::chrono::system_clock::now();
    float deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f;

    _perfStats.sceneUpdateTime = deltaTime;
}

AllocatedBuffer LunaticEngine::ReadGPUBuffer(VkCommandBuffer cmd, AllocatedBuffer& gpuBuffer, uint32_t size)
{
    AllocatedBuffer stagingBuffer = CreateBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY, "staging read GPU", 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VkBufferCopy copyInfo{};
    copyInfo.dstOffset = 0;
    copyInfo.srcOffset = 0;
    copyInfo.size = size;
    vkCmdCopyBuffer(cmd, gpuBuffer.buffer, stagingBuffer.buffer, 1, &copyInfo);

    return stagingBuffer;
}

void LunaticEngine::WriteMaterial(VkDevice device, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
    VkDescriptorImageInfo colorInfo{};
    colorInfo.sampler = resources.colorSampler;
    colorInfo.imageView = resources.colorImage.imageView;
    colorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo metalRoughInfo{};
    metalRoughInfo.sampler = resources.metalRoughSampler;
    metalRoughInfo.imageView = resources.metalRoughImage.imageView;
    metalRoughInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo normalInfo{};
    normalInfo.sampler = resources.normalSampler;
    normalInfo.imageView = resources.normalImage.imageView;
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    ImageMetaInfo imageMetaInfo
    {
        .constants = resources.constants
    };

    std::vector<VkDescriptorImageInfo>& imageInfos = _renderGraph.GetMultiImage(_allMaterialImages);
    imageInfos.push_back(colorInfo);
    imageInfos.push_back(metalRoughInfo); // So materialIndex + 1 will be how we access this
    imageInfos.push_back(normalInfo); // And materialIndex + 2 will be how we access this
    _allImageMetaInfo.push_back(imageMetaInfo);

    _renderGraph.AddDirtyResource(&_imageMetaInfoBuffer);
    _renderGraph.AddDirtyMultiImage(&_allMaterialImages);
}

float Halton(uint32_t i, uint32_t b)
{
    float f = 1.0f;
    float r = 0.0f;

    while (i > 0)
    {
        f /= static_cast<float>(b);
        r = r + f * static_cast<float>(i % b);
        i = static_cast<uint32_t>(floorf(static_cast<float>(i) / static_cast<float>(b)));
    }

    return r;
}