# Lunatic Engine
This is a real-time renderer I'm developing using the Vulkan API in C++. The initial GLTF loading, resource and descriptor abstractions, and boilerplate Vulkan initialization was taken from https://vkguide.dev/ and uses vk-bootstrap.

I've implemented features including bindless resources (textures and vertices), physically-based rendering with Cook-Torrance BRDF, GPU frustum culling, cascade shadow maps, reflective shadow maps, temporal anti-aliasing, screen-space ambient occlusion, and a deferred rendering model for opaque meshes.

There is also support to load the engine's DLL into Unreal Engine, and stream scene data from UE directly into the renderer and update it at runtime.

[Demo](https://youtu.be/7D-H_iwujLQ?si=OVbczDKzobxBHonX)

## Deferred Rendering & Physically-Based Rendering
To deter the performance impact that overdraw has with numerous lights in the scene, a G-buffer is first created consisting of world positions, normals, albedo, and other material properties of the final fragment. The images from this G-buffer are then evaluated in the lighting pass to ensure that only NUM_OF_LIGHTS * SCREEN_WIDTH * SCREEN_HEIGHT number of light calculations are performed. The Cook-Torrance BRDF model is used for physically based rendering when calculating the light contribution for each fragment.

- [vk_gbufferpass.cpp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/src/vk_gbufferpass.cpp)
- [vk_lightingpass.cpp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/src/vk_lightingpass.cpp)
- [gbuffer.vert](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/gbuffer.vert)
- [gbuffer.frag](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/gbuffer.frag)
- [lighting_deferred.vert](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/lighting_deferred.vert)
- [lighting_deferred.frag](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/lighting_deferred.frag)

## GPU Frustum Culling
Frustum culling is performed on the GPU through a compute shader. As the renderer runs with bindless resources, a buffer of VkDrawIndexedIndirectCommands are created and modified to handle which draws are performed and which are culled. This is determined by projecting the bounding radius of each mesh section into clip space, then determining if they're visible in the frustum from there. If culled, the mesh section's corresponding indirect draw command will have its instance count set to 0.
- [vk_frustumcompute.cpp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/src/vk_frustumcompute.cpp)
- [frustum_cull.comp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/frustum_cull.comp)

## Cascade Shadow Maps
The camera frustum is split into NUM_CASCADES (4) partitions along the view Z-axis to create a view-projection matrix from the perspective of the directional light. Each view-projection matrix is then used to generate a depth buffer from the perspective of the light source, which is then used to determine occlusion of a fragment during the lighting pass.

To avoid shimmering during both camera rotation and translation, the view-projections are aligned to the nearest texel size and the bounds of the cascades are set to the max distance between each partitioned frustum's corners.

- [vk_engine.cpp](https://github.com/HudsonHN/Lunatic_Engine/blob/ea589155acad96db80fa4848f2e32085dd949874/src/vk_engine.cpp#L2306)
- [vk_cascadeshadowpass.cpp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/src/vk_cascadeshadowpass.cpp)
- [light_depth.vert](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/light_depth.vert)
- [lighting_deferred.frag](https://github.com/HudsonHN/Lunatic_Engine/blob/ea589155acad96db80fa4848f2e32085dd949874/shaders/lighting_deferred.frag#L123)

## Reflective Shadow Maps
Indirect lighting is approximated through use of reflective shadow maps. The same process for generating cascades for the shadow maps is used, but this time instead of only generating a depth buffer for each cascade, a lower resolution G-buffer consisting of world positions, normals, and flux (albedo, light color, and intensity) is created. These buffers are then sampled using a weighted disk Hammersley sequence to determine attenuation for each fragment. The results are also temporally accumulated and then passed through a series of A-Trous edge-aware filtering to soften the noise from low sample count.

- [vk_engine.cpp](https://github.com/HudsonHN/Lunatic_Engine/blob/ea589155acad96db80fa4848f2e32085dd949874/src/vk_engine.cpp#L2306)
- [vk_reflectiveshadowpass.cpp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/src/vk_reflectiveshadowpass.cpp)
- [light_reflective_shadows.vert](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/light_reflective_shadows.vert)
- [light_reflective_shadows.frag](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/light_reflective_shadows.frag)
- [indirect_lighting_rsm.comp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/indirect_lighting_rsm.comp)
- [indirect_lighting_temporal_resolve.comp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/indirect_lighting_temporal_resolve.comp)
- [atrous_filter.frag](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/atrous_filter.frag)

## Temporal Anti-Aliasing
The clip-space vertex output is jittered in the G-buffer's vertex shader using a pre-computed Halton sequence. During the TAA resolve pass, the image for the current frame is combined with the output from the previous frame, which is sampled using Catmull-Rom filtering. Ghosting effects are reduced by using the G-buffer's world position image to back-project the fragment's current position into the previous frame's NDC space, the difference of which is stored in a velocity buffer that will then be used to calculate the UV for sampling the previous frame.

- [vk_taapass.cpp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/src/vk_taapass.cpp)
- [gbuffer.vert](https://github.com/HudsonHN/Lunatic_Engine/blob/ea589155acad96db80fa4848f2e32085dd949874/shaders/gbuffer.vert#L52)
- [temporal_anti_aliasing_resolve.frag](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/temporal_anti_aliasing_resolve.frag)

## Screen-Space Ambient Occlusion
Ambient light is occluded using a screen-space technique to add more depth to the final image for silhouettes and corners of meshes that naturally would have less light affecting them. The technique consists of sampling the depth buffer at various points around a unit hemisphere centered on the current fragment, which are then tested for occlusion against visible geometry by comparing their depths. A fragment with more of its samples occluded by geometry will have less ambient light contribution. The final output is then blurred to reduce noise from low sample counts.

- [vk_ssaopass.cpp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/src/vk_ssaopass.cpp)
- [vk_blurpass.cpp](https://github.com/HudsonHN/Lunatic_Engine/blob/main/src/vk_blurpass.cpp)
- [ssao.frag](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/ssao.frag)
- [blur_horizontal.frag](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/blur_horizontal.frag)
- [blur_vertical.frag](https://github.com/HudsonHN/Lunatic_Engine/blob/main/shaders/blur_vertical.frag)
