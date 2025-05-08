#pragma once

#include <queue>
#include "vk_engine.h"

#ifdef LUNATICENGINE_EXPORTS
#define LUNATIC_API __declspec(dllexport)
#else
#define LUNATIC_API __declspec(dllimport)
#endif

extern "C" {
	LUNATIC_API void AddVertex(const Vertex& vertices);
	LUNATIC_API void AddIndex(uint32_t indices);
	LUNATIC_API uint32_t GetTotalVertexCount();
	LUNATIC_API uint32_t GetTotalIndexCount();
	LUNATIC_API void AddIndirectDraw(const GeoSurface& surface);
	LUNATIC_API void AddMaterial(const MaterialConstants& constants);
	LUNATIC_API void AddTexture(void* data, uint32_t width, uint32_t height);
	LUNATIC_API void UploadStream();
	LUNATIC_API void UpdateTransform(uint32_t index, glm::mat4 mat);
	LUNATIC_API void ChangeSkybox(bool check);
	LUNATIC_API void InitInstance();
	LUNATIC_API void InitEngine();
	LUNATIC_API void RunEngine();
	LUNATIC_API void CloseEngine();
	std::queue<AllocatedImage> queuedImages;
	LunaticEngine* engineLink = nullptr;
}