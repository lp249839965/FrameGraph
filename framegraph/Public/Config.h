// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

// TODO
//#define FG_HALF_TYPE

namespace FG
{

	// buffer
	static constexpr unsigned	FG_MaxVertexBuffers			= 8;
	static constexpr unsigned	FG_MaxAttribs				= 16;

	// render pass
	static constexpr unsigned	FG_MaxViewports				= 16;
	static constexpr unsigned	FG_MaxColorBuffers			= 8;
	static constexpr unsigned	FG_MaxRenderPassSubpasses	= 8;

	// pipeline
	static constexpr unsigned	FG_MaxDescriptorSets		= 8;
	static constexpr unsigned	FG_MaxPushConstants			= 8;
	static constexpr unsigned	FG_MaxPushConstantsSize		= 128;	// bytes
	static constexpr unsigned	FG_MaxSpecConstants			= 16;
	static constexpr unsigned	FG_MaxBufferDynamicOffsets	= 8;

	// queue
	static constexpr unsigned	FG_MaxQueueFamilies			= 32;

	// task
	static constexpr unsigned	FG_MaxTaskDependencies		= 8;
	static constexpr unsigned	FG_MaxCopyRegions			= 8;
	static constexpr unsigned	FG_MaxClearRanges			= 8;
	static constexpr unsigned	FG_MaxBlitRegions			= 8;
	static constexpr unsigned	FG_MaxResolveRegions		= 8;

	// memory manager
	static constexpr unsigned	FG_VkHostWritePageSizeMb	= 256;	// Mb
	static constexpr unsigned	FG_VkDevicePageSizeMb		= 256;	// Mb

	// resource manager
	static constexpr unsigned	FG_MaxImageResources		= 32u << 10;
	static constexpr unsigned	FG_MaxBufferResources		= 32u << 10;
	static constexpr unsigned	FG_MaxPipelines				= 1u << 10;

	// frame graph
	static constexpr unsigned	FG_MaxRingBufferSize		= 8;
	static constexpr unsigned	FG_MaxThreadDependencies	= 8;


}	// FG