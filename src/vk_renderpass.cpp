#include "vk_renderpass.h"

RenderPass::~RenderPass()
{
	deletionQueue.Flush();
}

void RenderPass::Setup(LunaticEngine* engine, DescriptorAllocatorGrowable* descriptorAllocator)
{
	DataSetup(engine);
	DescriptorSetup(engine, descriptorAllocator);
	PipelineSetup(engine);
}