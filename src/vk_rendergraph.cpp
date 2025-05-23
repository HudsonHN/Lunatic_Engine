#include "vk_rendergraph.h"
#include "vk_engine.h"
#include "vk_renderpass.h"
#include "vk_frustumcompute.h"
#include "vk_gbufferpass.h"
#include "vk_cascadeshadowpass.h"
#include "vk_reflectiveshadowpass.h"
#include "vk_ssaopass.h"
#include "vk_indirectlightingrsmcompute.h"
#include "vk_indirectlightingaccumulation.h"
#include "vk_atrousfilterpass.h"
#include "vk_blurpass.h"
#include "vk_lightingpass.h"
#include "vk_taapass.h"
#include "vk_skyboxpass.h"
#include "vk_transparentpass.h"
#include "vk_tonemappingpass.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include "vk_pipelines.h"

ResourceHandle RenderGraph::CreateCubemap(std::string paths[6])
{
	ResourceHandle handle{};
	handle.index = static_cast<uint32_t>(images.size());
	images.emplace_back(engine->LoadCubemap(paths));

	return handle;
}

ResourceHandle RenderGraph::CreateImage(std::string name, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
	bool mipmapped, VkImageViewType viewType, VkImageCreateFlags createFlags, uint32_t arrayLayers)
{
	ResourceHandle handle{};
	handle.index = static_cast<uint32_t>(images.size());
	images.emplace_back(engine->CreateImage(name, size, format, usage, mipmapped, viewType, createFlags, arrayLayers));

	return handle;
}

ResourceHandle RenderGraph::CreateImage(std::string name, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
	bool mipmapped, VkImageViewType viewTypes, VkImageCreateFlags createFlags, uint32_t arrayLayers)
{
	ResourceHandle handle{};
	handle.index = static_cast<uint32_t>(images.size());
	images.emplace_back(engine->CreateImage(name, data, size, format, usage, mipmapped, viewTypes, createFlags, arrayLayers));

	return handle;
}

ResourceHandle RenderGraph::CreateImageFromFile(const char* file, VkFormat format, VkImageUsageFlags usageFlags, bool mipmapped)
{
	ResourceHandle handle{};
	handle.index = static_cast<uint32_t>(images.size());
	images.emplace_back(engine->LoadImageFromFile(file, format, usageFlags, mipmapped));

	return handle;
}

void RenderGraph::DestroyImage(const ResourceHandle& handle)
{
	AllocatedImage& image = GetImage(handle);
	images.erase(images.begin() + handle.index);
	engine->DestroyImage(image);
}

void RenderGraph::DestroyBuffer(const ResourceHandle& handle)
{
	AllocatedBuffer& buffer = GetBuffer(handle);
	buffers.erase(buffers.begin() + handle.index);
	engine->DestroyBuffer(buffer);
}

ResourceHandle RenderGraph::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, std::string name)
{
	ResourceHandle handle{};
	handle.index = static_cast<uint32_t>(buffers.size());
	handle.allocSize = static_cast<uint32_t>(allocSize);
	buffers.emplace_back(engine->CreateBuffer(allocSize, usage, memoryUsage, name));

	return handle;
}

ResourceHandle RenderGraph::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, std::string name, VkMemoryPropertyFlags propertyFlags)
{
	ResourceHandle handle{};
	handle.index = static_cast<uint32_t>(buffers.size());
	handle.allocSize = static_cast<uint32_t>(allocSize);
	buffers.emplace_back(engine->CreateBuffer(allocSize, usage, memoryUsage, name, propertyFlags));

	return handle;
}

MultiImageHandle RenderGraph::CreateMultiImage()
{
	MultiImageHandle handle{};
	handle.index = static_cast<uint32_t>(multiImages.size());
	std::vector<VkDescriptorImageInfo> imageInfos;
	multiImages.emplace_back(imageInfos);

	return handle;
}

void RenderGraph::DestroyMultiImage(const MultiImageHandle& handle)
{
	multiImages.erase(multiImages.begin() + handle.index);
}

void RenderGraph::AddSceneDataDependantPass(RenderPass* pass)
{
	pass->sceneDataDescriptorSetLayout = sceneDataDescriptorSetLayout;
	sceneDataDependantPasses.push_back(pass);
}

void RenderGraph::Setup()
{
	if (!bindlessAllocator || !globalAllocator)
	{
		fmt::println("Cannot initialize render graph because allocators are not assigned!");
		return;
	}

	DescriptorLayoutBuilder builder;
	builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
	sceneDataDescriptorSetLayout = builder.Build(engine->_device);
	engine->_mainDeletionQueue.PushFunction([=]() {
		vkDestroyDescriptorSetLayout(engine->_device, sceneDataDescriptorSetLayout, nullptr);
	});

	frustumCompute = dynamic_cast<FrustumCompute*>(renderPasses.emplace_back(new FrustumCompute()));
	gBufferPass = dynamic_cast<GBufferPass*>(renderPasses.emplace_back(new GBufferPass()));
	cascadeShadowPass = dynamic_cast<CascadeShadowPass*>(renderPasses.emplace_back(new CascadeShadowPass()));
	reflectiveShadowPass = dynamic_cast<ReflectiveShadowPass*>(renderPasses.emplace_back(new ReflectiveShadowPass()));
	ssaoPass = dynamic_cast<SSAOPass*>(renderPasses.emplace_back(new SSAOPass()));
	blurPass = dynamic_cast<BlurPass*>(renderPasses.emplace_back(new BlurPass()));
	indirectLightingRSMCompute = dynamic_cast<IndirectLightingRSMCompute*>(renderPasses.emplace_back(new IndirectLightingRSMCompute()));
	indirectLightingAccumulationCompute = dynamic_cast<IndirectLightingAccumulationCompute*>(renderPasses.emplace_back(new IndirectLightingAccumulationCompute()));
	atrousFilterPass = dynamic_cast<AtrousFilterPass*>(renderPasses.emplace_back(new AtrousFilterPass()));
	lightingPass = dynamic_cast<LightingPass*>(renderPasses.emplace_back(new LightingPass()));
	taaPass = dynamic_cast<TAAPass*>(renderPasses.emplace_back(new TAAPass()));
	skyboxPass = dynamic_cast<SkyboxPass*>(renderPasses.emplace_back(new SkyboxPass()));
	transparentPass = dynamic_cast<TransparentPass*>(renderPasses.emplace_back(new TransparentPass()));
	tonemappingPass = dynamic_cast<TonemappingPass*>(renderPasses.emplace_back(new TonemappingPass()));

	for (RenderPass* renderPass : renderPasses)
	{
		renderPass->renderGraph = this;
	}

	AddSceneDataDependantPass(frustumCompute);
	frustumCompute->Setup(engine, bindlessAllocator);

	AddSceneDataDependantPass(gBufferPass);
	gBufferPass->Setup(engine, bindlessAllocator);

	cascadeShadowPass->Setup(engine, bindlessAllocator);

	AddSceneDataDependantPass(reflectiveShadowPass);
	reflectiveShadowPass->Setup(engine, bindlessAllocator);

	AddSceneDataDependantPass(ssaoPass);
	ssaoPass->Setup(engine, bindlessAllocator);

	blurPass->Setup(engine, globalAllocator);

	AddSceneDataDependantPass(indirectLightingRSMCompute);
	indirectLightingRSMCompute->Setup(engine, bindlessAllocator);

	AddSceneDataDependantPass(indirectLightingAccumulationCompute);
	indirectLightingAccumulationCompute->Setup(engine, bindlessAllocator);

	atrousFilterPass->Setup(engine, globalAllocator);

	AddSceneDataDependantPass(lightingPass);
	lightingPass->Setup(engine, bindlessAllocator);

	taaPass->Setup(engine, bindlessAllocator);

	skyboxPass->Setup(engine, globalAllocator);

	AddSceneDataDependantPass(transparentPass);
	transparentPass->Setup(engine, bindlessAllocator);

	tonemappingPass->Setup(engine, globalAllocator);
}

void RenderGraph::Cleanup()
{
	for (AllocatedImage& image : images)
	{
		engine->DestroyImage(image);
	}
	for (AllocatedBuffer& buffer : buffers)
	{
		engine->DestroyBuffer(buffer);
	}
	for (RenderPass* renderPass : renderPasses)
	{
		delete renderPass;
	}
}

void RenderGraph::Run(VkCommandBuffer cmd, uint32_t swapchainImageIndex)
{
	UpdateDescriptorSets();
	
	AllocatedBuffer gpuSceneDataBuffer = engine->CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, "gpu scene data buffer");

	// Add it to the deletion queue of this frame so it gets deleted once its been used
	engine->GetCurrentFrame().deletionQueue.PushFunction([=, this]() {
		engine->DestroyBuffer(gpuSceneDataBuffer);
		});

	// Write the buffer
	void* gpuSceneData = nullptr;
	vmaMapMemory(engine->_allocator, gpuSceneDataBuffer.allocation, &gpuSceneData);
	memcpy(gpuSceneData, &engine->_sceneData, sizeof(GPUSceneData));
	vmaUnmapMemory(engine->_allocator, gpuSceneDataBuffer.allocation);

	// Create a descriptor set that binds that buffer and Update it
	// GPU scene data descriptor set
	sceneDataDescriptorSet = engine->GetCurrentFrame().frameDescriptors.Allocate(engine->_device, sceneDataDescriptorSetLayout);
	{
		DescriptorWriter writer;
		writer.WriteBuffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		writer.UpdateSet(engine->_device, sceneDataDescriptorSet);
	}

	for (RenderPass* renderPass : sceneDataDependantPasses)
	{
		renderPass->sceneDataDescriptorSet = sceneDataDescriptorSet;
	}

	vkutil::TransitionImage(cmd, GetImage(engine->_drawImage).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkutil::TransitionImage(cmd, GetImage(engine->_depthImage).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	frustumCompute->Execute(engine, cmd);
	{
		VkBufferMemoryBarrier2 triangleBarrier = vkinit::pipeline_buffer_memory_barrier(
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
			GetBuffer(engine->_gpuTriangleCountBuffer).buffer, sizeof(uint32_t));

		VkBufferMemoryBarrier2 opaqueCmdBarrier = vkinit::pipeline_buffer_memory_barrier(
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
			GetBuffer(engine->_opaqueDrawCommandBuffer).buffer, GetBuffer(engine->_opaqueDrawCommandBuffer).usedSize);

		VkBufferMemoryBarrier2 transparentCmdBarrier = vkinit::pipeline_buffer_memory_barrier(
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
			GetBuffer(engine->_transparentDrawCommandBuffer).buffer, GetBuffer(engine->_transparentDrawCommandBuffer).usedSize);

		VkBufferMemoryBarrier2 bufferBarriers[] = { triangleBarrier, opaqueCmdBarrier, transparentCmdBarrier };

		VkDependencyInfo dependencyInfo{};
		dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependencyInfo.bufferMemoryBarrierCount = 2;
		dependencyInfo.pBufferMemoryBarriers = bufferBarriers;

		vkCmdPipelineBarrier2(cmd, &dependencyInfo);
	}

	VkBufferCopy copyInfo{};
	copyInfo.dstOffset = 0;
	copyInfo.srcOffset = 0;
	copyInfo.size = sizeof(uint32_t);
	vkCmdCopyBuffer(cmd, GetBuffer(engine->_gpuTriangleCountBuffer).buffer, GetBuffer(engine->_cpuTriangleCountBuffer).buffer, 1, &copyInfo);

	if (engine->_bDrawDirectionalShadows)
	{
		cascadeShadowPass->Execute(engine, cmd);
		if (engine->_bDrawReflectiveDirectionalShadows)
		{
			reflectiveShadowPass->Execute(engine, cmd);
		}
	}

	vkutil::TransitionImage(cmd, GetImage(engine->_positionColor).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::TransitionImage(cmd, GetImage(engine->_normalColor).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::TransitionImage(cmd, GetImage(engine->_albedoSpecColor).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::TransitionImage(cmd, GetImage(engine->_metalRoughnessColor).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::TransitionImage(cmd, GetImage(engine->_velocityImage).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	gBufferPass->Execute(engine, cmd);

	vkutil::TransitionImage(cmd, GetImage(engine->_positionColor).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::TransitionImage(cmd, GetImage(engine->_normalColor).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::TransitionImage(cmd, GetImage(engine->_albedoSpecColor).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::TransitionImage(cmd, GetImage(engine->_metalRoughnessColor).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkutil::TransitionImage(cmd, GetImage(engine->_velocityImage).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);


	if (engine->_bDrawDirectionalShadows && engine->_bDrawReflectiveDirectionalShadows)
	{
		if (engine->_bUseIndirectLightingCompute)
		{
			vkutil::TransitionImage(cmd, GetImage(engine->_indirectLightColor).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
			indirectLightingRSMCompute->Execute(engine, cmd);
			vkutil::TransitionImage(cmd, GetImage(engine->_indirectLightColor).image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		}
		else
		{
			vkutil::TransitionImage(cmd, GetImage(engine->_indirectLightColor).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
			vkutil::TransitionImage(cmd, GetImage(engine->_indirectLightColor).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		}
		if (engine->_bApplyTAA && engine->_bApplyIndirectAccumulation)
		{
			vkutil::TransitionImage(cmd, GetImage(engine->_historyIndirectLightColor).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

			indirectLightingAccumulationCompute->Execute(engine, cmd);

			vkutil::TransitionImage(cmd, GetImage(engine->_historyIndirectLightColor).image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			vkutil::TransitionImage(cmd, GetImage(engine->_indirectLightColor).image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			vkutil::TransitionImage(cmd, GetImage(engine->_prevIndirectLightColor).image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			vkutil::CopyImageToImage(cmd, GetImage(engine->_historyIndirectLightColor).image, GetImage(engine->_indirectLightColor).image, engine->_drawExtent, engine->_drawExtent);
			vkutil::CopyImageToImage(cmd, GetImage(engine->_historyIndirectLightColor).image, GetImage(engine->_prevIndirectLightColor).image, engine->_drawExtent, engine->_drawExtent);
			vkutil::TransitionImage(cmd, GetImage(engine->_indirectLightColor).image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			vkutil::TransitionImage(cmd, GetImage(engine->_prevIndirectLightColor).image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		if (engine->_bApplyAtrousFilter)
		{
			atrousFilterPass->Execute(engine, cmd);
		}
	}
	else
	{
		vkutil::TransitionImage(cmd, GetImage(engine->_indirectLightColor).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	if (engine->_bApplySSAO)
	{
		vkutil::TransitionImage(cmd, GetImage(engine->_ssaoColor).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		ssaoPass->Execute(engine, cmd);
		vkutil::TransitionImage(cmd, GetImage(engine->_ssaoColor).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		if (engine->_bApplySSAOBlur)
		{
			blurPass->Execute(engine, cmd);
		}
	}
	else
	{
		vkutil::TransitionImage(cmd, GetImage(engine->_ssaoColor).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	lightingPass->Execute(engine, cmd);

	if (engine->_bApplyTAA)
	{
		vkutil::TransitionImage(cmd, GetImage(engine->_drawImage).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkutil::TransitionImage(cmd, GetImage(engine->_historyImage).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		taaPass->Execute(engine, cmd);

		vkutil::TransitionImage(cmd, GetImage(engine->_historyImage).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		vkutil::TransitionImage(cmd, GetImage(engine->_drawImage).image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkutil::CopyImageToImage(cmd, GetImage(engine->_historyImage).image, GetImage(engine->_drawImage).image, engine->_drawExtent, engine->_drawExtent);

		vkutil::TransitionImage(cmd, GetImage(engine->_drawImage).image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	}

	skyboxPass->Execute(engine, cmd);
	transparentPass->Execute(engine, cmd);

	if (engine->_bApplyTAA)
	{
		vkutil::TransitionImage(cmd, GetImage(engine->_drawImage).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		vkutil::TransitionImage(cmd, GetImage(engine->_prevFrameImage).image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkutil::CopyImageToImage(cmd, GetImage(engine->_drawImage).image, GetImage(engine->_prevFrameImage).image, engine->_drawExtent, engine->_drawExtent);
		vkutil::TransitionImage(cmd, GetImage(engine->_prevFrameImage).image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	if (engine->_bApplyTonemap)
	{
		if (engine->_bApplyTAA)
		{
			vkutil::TransitionImage(cmd, GetImage(engine->_drawImage).image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		else
		{
			vkutil::TransitionImage(cmd, GetImage(engine->_drawImage).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		vkutil::TransitionImage(cmd, GetImage(engine->_intermediateImage).image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		tonemappingPass->Execute(engine, cmd);

		vkutil::TransitionImage(cmd, GetImage(engine->_intermediateImage).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		vkutil::TransitionImage(cmd, engine->_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkutil::CopyImageToImage(cmd, GetImage(engine->_intermediateImage).image, engine->_swapchainImages[swapchainImageIndex], engine->_drawExtent, engine->_swapchainExtent);
	}
	else
	{
		if (!engine->_bApplyTAA)
		{
			vkutil::TransitionImage(cmd, GetImage(engine->_drawImage).image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		}
		vkutil::TransitionImage(cmd, engine->_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		vkutil::CopyImageToImage(cmd, GetImage(engine->_drawImage).image, engine->_swapchainImages[swapchainImageIndex], engine->_drawExtent, engine->_swapchainExtent);
	}

	vkutil::TransitionImage(cmd, GetImage(engine->_velocityImage).image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::TransitionImage(cmd, GetImage(engine->_prevVelocityImage).image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkutil::CopyImageToImage(cmd, GetImage(engine->_velocityImage).image, GetImage(engine->_prevVelocityImage).image, engine->_drawExtent, engine->_drawExtent);
	vkutil::TransitionImage(cmd, GetImage(engine->_prevVelocityImage).image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkutil::TransitionImage(cmd, engine->_swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

AllocatedBuffer& RenderGraph::GetBuffer(const ResourceHandle& handle)
{
	return buffers[handle.index];
}

AllocatedImage& RenderGraph::GetImage(const ResourceHandle& handle)
{
	return images[handle.index];
}

std::vector<VkDescriptorImageInfo>& RenderGraph::GetMultiImage(const MultiImageHandle& handle)
{
	return multiImages[handle.index];
}

void RenderGraph::AddDirtyResource(ResourceHandle* handle)
{
	dirtyResources.insert(handle);
}

void RenderGraph::AddDirtyMultiImage(MultiImageHandle* handle)
{
	dirtyMultiImages.insert(handle);
}

void RenderGraph::UpdateDescriptorSets()
{
	for (ResourceHandle* resourceHandle : dirtyResources)
	{
		for (DescriptorBindingInfo& bindingInfo : resourceHandle->bindingInfos)
		{
			DescriptorWriter writer;
			if (bindingInfo.isImage)
			{
				AllocatedImage& image = GetImage(*resourceHandle);
				writer.WriteImage(bindingInfo.binding, image.imageView, bindingInfo.imageSampler, bindingInfo.imageLayout, bindingInfo.type);
			}
			else
			{
				AllocatedBuffer& buffer = GetBuffer(*resourceHandle);
				writer.WriteBuffer(bindingInfo.binding, buffer.buffer, buffer.usedSize, 0, bindingInfo.type);
			}
			writer.UpdateSet(engine->_device, bindingInfo.descriptorSet);
		}
	}
	dirtyResources.clear();

	for (MultiImageHandle* multiImageHandle : dirtyMultiImages)
	{
		std::vector<VkDescriptorImageInfo>& imageInfos = GetMultiImage(*multiImageHandle);
		for (MultiDescriptorBindingInfo& bindingInfo : multiImageHandle->bindingInfos)
		{
			DescriptorWriter writer;
			writer.WriteImages(bindingInfo.binding, imageInfos, bindingInfo.type);
			writer.UpdateSet(engine->_device, bindingInfo.descriptorSet);
		}
	}
	dirtyMultiImages.clear();
}