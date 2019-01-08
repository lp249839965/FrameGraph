// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "EShaderCompilationFlags.h"
#include <mutex>

namespace FG
{

	//
	// Vulkan Pipeline Compiler
	//

	class VPipelineCompiler final : public IPipelineCompiler
	{
	// types
	private:
		using StringShaderData	= PipelineDescription::SharedShaderPtr< String >;
		using BinaryShaderData	= PipelineDescription::SharedShaderPtr< Array<uint> >;
		using VkShaderPtr		= PipelineDescription::VkShaderPtr;

		struct BinaryShaderDataHash {
			ND_ size_t  operator () (const BinaryShaderData &value) const noexcept {
				return value->GetHashOfData();
			}
		};

		using ShaderCache_t		= HashMap< BinaryShaderData, VkShaderPtr >;
		using ShaderDataMap_t	= PipelineDescription::ShaderDataMap_t;


	// variables
	private:
		std::mutex							_lock;	
		UniquePtr< class SpirvCompiler >	_spirvCompiler;
		ShaderCache_t						_shaderCache;
		EShaderCompilationFlags				_compilerFlags			= Default;

		// immutable:
		PhysicalDeviceVk_t					_physicalDevice;
		DeviceVk_t							_logicalDevice;
		void *								_fpCreateShaderModule	= null;
		void *								_fpDestroyShaderModule	= null;


	// methods
	public:
		VPipelineCompiler ();
		VPipelineCompiler (PhysicalDeviceVk_t physicalDevice, DeviceVk_t device);
		~VPipelineCompiler ();

		bool SetCompilationFlags (EShaderCompilationFlags flags);

		ND_ EShaderCompilationFlags  GetCompilationFlags () const	{ return _compilerFlags; }

		void ReleaseUnusedShaders ();
		void ReleaseShaderCache ();

		bool IsSupported (const MeshPipelineDesc &ppln, EShaderLangFormat dstFormat) const override;
		bool IsSupported (const RayTracingPipelineDesc &ppln, EShaderLangFormat dstFormat) const override;
		bool IsSupported (const GraphicsPipelineDesc &ppln, EShaderLangFormat dstFormat) const override;
		bool IsSupported (const ComputePipelineDesc &ppln, EShaderLangFormat dstFormat) const override;
		
		bool Compile (INOUT MeshPipelineDesc &ppln, EShaderLangFormat dstFormat) override;
		bool Compile (INOUT RayTracingPipelineDesc &ppln, EShaderLangFormat dstFormat) override;
		bool Compile (INOUT GraphicsPipelineDesc &ppln, EShaderLangFormat dstFormat) override;
		bool Compile (INOUT ComputePipelineDesc &ppln, EShaderLangFormat dstFormat) override;


	private:
		bool _MergePipelineResources (const PipelineDescription::PipelineLayout &srcLayout,
									  INOUT PipelineDescription::PipelineLayout &dstLayout) const;

		bool _MergeUniforms (const PipelineDescription::UniformMap_t &srcUniforms,
							 INOUT PipelineDescription::UniformMap_t &dstUniforms) const;

		bool _CreateVulkanShader (INOUT PipelineDescription::Shader &shader);

		static bool _IsSupported (const ShaderDataMap_t &data);
	};


}	// FG
