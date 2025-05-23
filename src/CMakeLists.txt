
# Add source to this project's executable.
add_executable (engine
#add_library (engine SHARED
  main.cpp
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h
  vk_images.h
  vk_images.cpp 
  vk_descriptors.h
  vk_descriptors.cpp
  vk_pipelines.h
  vk_pipelines.cpp
  vk_engine.h
  vk_engine.cpp
  vk_loader.h
  vk_loader.cpp
  camera.cpp
  camera.h
  vk_ueplugin.h
  vk_ueplugin.cpp
  vk_rendergraph.h
  vk_rendergraph.cpp
  vk_renderpass.h
  vk_renderpass.cpp
  vk_gbufferpass.h
  vk_gbufferpass.cpp
  vk_transparentpass.h
  vk_transparentpass.cpp
  vk_lightingpass.h
  vk_lightingpass.cpp
  vk_cascadeshadowpass.h
  vk_cascadeshadowpass.cpp
  vk_reflectiveshadowpass.h
  vk_reflectiveshadowpass.cpp
  vk_skyboxpass.h
  vk_skyboxpass.cpp
  vk_ssaopass.h
  vk_ssaopass.cpp
  vk_frustumcompute.h
  vk_frustumcompute.cpp
  vk_indirectlightingrsmcompute.h
  vk_indirectlightingrsmcompute.cpp
  vk_indirectlightingaccumulation.h
  vk_indirectlightingaccumulation.cpp
  vk_atrousfilterpass.h
  vk_atrousfilterpass.cpp
  vk_blurpass.h
  vk_blurpass.cpp
  vk_taapass.h
  vk_taapass.cpp
  vk_tonemappingpass.h
  vk_tonemappingpass.cpp
  vk_debug.h
  vk_debug.cpp
)

set_property(TARGET engine PROPERTY CXX_STANDARD 20)
target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(engine PRIVATE LUNATICENGINE_EXPORTS)
target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(engine PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)

target_precompile_headers(engine PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.h>)

add_custom_command(TARGET engine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>
  COMMAND_EXPAND_LISTS
  )