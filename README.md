This is a real-time renderer I'm developing using the Vulkan API in C++. The inital GLTF loading, resource and descriptor abstractions, and boilerplate Vulkan initialization was taken from https://vkguide.dev/ and uses vk-bootstrap.

I've implemented features including bindless resources (textures and vertices), physically-based rendering with Cook-Torrance BRDF, compute shader frustum culling, cascade shadow maps, reflective shadow maps, temporal anti-aliasing, screen-space ambient occlusion, and follows a deferred rendering model for opaque meshes.
There is also support to load the engine's DLL into Unreal Engine, and stream scene data from UE directly into the renderer and update it at runtime.

Future TODOs:
- A render graph abstraction to automatically handle resource management and memory aliasing
- DDGI to replace RSM indirect lighting
- Screen-space reflections
- Animations
