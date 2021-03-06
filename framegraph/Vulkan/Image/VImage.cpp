// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "VImage.h"
#include "VEnumCast.h"
#include "FGEnumCast.h"
#include "VDevice.h"
#include "VMemoryObj.h"
#include "VResourceManager.h"

namespace FG
{
namespace
{	
/*
=================================================
	_ChooseAspect
=================================================
*/
	ND_ static VkImageAspectFlagBits  ChooseAspect (EPixelFormat format)
	{
		VkImageAspectFlagBits	result = Zero;

		if ( EPixelFormat_IsColor( format ))
			result |= VK_IMAGE_ASPECT_COLOR_BIT;
		else
		{
			if ( EPixelFormat_HasDepth( format ))
				result |= VK_IMAGE_ASPECT_DEPTH_BIT;

			if ( EPixelFormat_HasStencil( format ))
				result |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		return result;
	}
	
/*
=================================================
	_ChooseDefaultLayout
=================================================
*/
	ND_ static VkImageLayout  ChooseDefaultLayout (EImageUsage usage, VkImageLayout defaultLayout)
	{
		VkImageLayout	result = VK_IMAGE_LAYOUT_GENERAL;

		if ( defaultLayout != VK_IMAGE_LAYOUT_MAX_ENUM and defaultLayout != VK_IMAGE_LAYOUT_UNDEFINED )
			result = defaultLayout;
		else
		// render target layouts has high priority to avoid unnecessary decompressions
		if ( AllBits( usage, EImageUsage::ColorAttachment ))
			result = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		else
		if ( AllBits( usage, EImageUsage::DepthStencilAttachment ))
			result = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		else
		if ( AllBits( usage, EImageUsage::Sampled ))
			result = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		else
		if ( AllBits( usage, EImageUsage::Storage ))
			result = VK_IMAGE_LAYOUT_GENERAL;

		return result;
	}
	
/*
=================================================
	GetAllImageAccessMasks
=================================================
*/
	ND_ static VkAccessFlagBits  GetAllImageAccessMasks (VkImageUsageFlags usage)
	{
		VkAccessFlagBits	result = Zero;

		for (VkImageUsageFlags t = 1; t <= usage; t <<= 1)
		{
			if ( not AllBits( usage, t ))
				continue;

			BEGIN_ENUM_CHECKS();
			switch ( VkImageUsageFlagBits(t) )
			{
				case VK_IMAGE_USAGE_TRANSFER_SRC_BIT :				result |= VK_ACCESS_TRANSFER_READ_BIT;					break;
				case VK_IMAGE_USAGE_TRANSFER_DST_BIT :				break;
				case VK_IMAGE_USAGE_SAMPLED_BIT :					result |= VK_ACCESS_SHADER_READ_BIT;					break;
				case VK_IMAGE_USAGE_STORAGE_BIT :					result |= VK_ACCESS_SHADER_READ_BIT;					break;
				case VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT :			result |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;			break;
				case VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT :	result |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;	break;
				case VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT :		break;
				case VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT :			result |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;			break;

				#ifdef VK_NV_shading_rate_image
				case VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV :		result |= VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV;		break;
				#endif	
				#ifdef VK_EXT_fragment_density_map
				case VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT :
				#endif

				case VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM :			break;	// to shutup compiler warnings
			}
			END_ENUM_CHECKS();
		}
		return result;
	}

}	// namespace
//-----------------------------------------------------------------------------



/*
=================================================
	destructor
=================================================
*/
	VImage::~VImage ()
	{
		ASSERT( _image == VK_NULL_HANDLE );
		ASSERT( _viewMap.empty() );
		ASSERT( not _memoryId );
	}

/*
=================================================
	Create
=================================================
*/
	bool VImage::Create (VResourceManager &resMngr, const ImageDesc &desc, RawMemoryID memId, VMemoryObj &memObj,
						 EQueueFamilyMask queueFamilyMask, EResourceState defaultState, StringView dbgName)
	{
		EXLOCK( _drCheck );
		CHECK_ERR( _image == VK_NULL_HANDLE );
		CHECK_ERR( not _memoryId );
		CHECK_ERR( not desc.isExternal );
		CHECK_ERR( desc.format != Default );
		CHECK_ERR( desc.usage != Default );
		
		auto&		dev			= resMngr.GetDevice();
		const bool	opt_tiling	= not uint(memObj.MemoryType() & EMemoryTypeExt::HostVisible);

		_desc		= desc;		_desc.Validate();
		_memoryId	= MemoryID{ memId };
		
		ASSERT( IsSupported( dev, _desc, EMemoryType(memObj.MemoryType()) ));

		// create image
		VkImageCreateInfo	info = {};
		info.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext			= null;
		info.flags			= VEnumCast( _desc.flags );
		info.imageType		= VEnumCast( _desc.imageType );
		info.format			= VEnumCast( _desc.format );
		info.extent.width	= _desc.dimension.x;
		info.extent.height	= _desc.dimension.y;
		info.extent.depth	= _desc.dimension.z;
		info.mipLevels		= _desc.maxLevel.Get();
		info.arrayLayers	= _desc.arrayLayers.Get();
		info.samples		= VEnumCast( _desc.samples );
		info.tiling			= (opt_tiling ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR);
		info.usage			= VEnumCast( _desc.usage );
		info.initialLayout	= (opt_tiling ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED);

		StaticArray<uint32_t, 8>	queue_family_indices = {};

		// setup sharing mode
		if ( queueFamilyMask != Default )
		{
			info.sharingMode			= VK_SHARING_MODE_CONCURRENT;
			info.pQueueFamilyIndices	= queue_family_indices.data();
			
			for (uint i = 0, mask = (1u<<i);
				 mask <= uint(queueFamilyMask) and info.queueFamilyIndexCount < queue_family_indices.size();
				 ++i, mask = (1u<<i))
			{
				if ( AllBits( queueFamilyMask, mask ))
					queue_family_indices[ info.queueFamilyIndexCount++ ] = i;
			}
		}

		// reset to exclusive mode
		if ( info.queueFamilyIndexCount < 2 )
		{
			info.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;
			info.pQueueFamilyIndices	= null;
			info.queueFamilyIndexCount	= 0;
		}

		VK_CHECK( dev.vkCreateImage( dev.GetVkDevice(), &info, null, OUT &_image ));

		CHECK_ERR( memObj.AllocateForImage( resMngr.GetMemoryManager(), _image ));
		
		if ( not dbgName.empty() )
		{
			dev.SetObjectName( BitCast<uint64_t>(_image), dbgName, VK_OBJECT_TYPE_IMAGE );
		}

		_readAccessMask		= GetAllImageAccessMasks( info.usage );
		_aspectMask			= ChooseAspect( _desc.format );
		_defaultLayout		= ChooseDefaultLayout( _desc.usage, EResourceState_ToImageLayout( defaultState, _aspectMask ));
		_queueFamilyMask	= queueFamilyMask;
		_debugName			= dbgName;

		return true;
	}

/*
=================================================
	Create
=================================================
*/
	bool VImage::Create (const VDevice &dev, const VulkanImageDesc &desc, StringView dbgName, OnRelease_t &&onRelease)
	{
		EXLOCK( _drCheck );
		CHECK_ERR( _image == VK_NULL_HANDLE );
		CHECK_ERR( desc.image );

		_image				= BitCast<VkImage>( desc.image );
		_desc.imageType		= FGEnumCast( BitCast<VkImageType>( desc.imageType ));
		_desc.flags			= FGEnumCast( BitCast<VkImageCreateFlagBits>( desc.flags ));
		_desc.dimension		= desc.dimension;
		_desc.format		= FGEnumCast( BitCast<VkFormat>( desc.format ));
		_desc.usage			= FGEnumCast( BitCast<VkImageUsageFlagBits>( desc.usage ));
		_desc.arrayLayers	= ImageLayer{ desc.arrayLayers };
		_desc.maxLevel		= MipmapLevel{ desc.maxLevels };
		_desc.samples		= FGEnumCast( BitCast<VkSampleCountFlagBits>( desc.samples ));
		_desc.isExternal	= true;
		
		ASSERT( IsSupported( dev, _desc, EMemoryType::Default ));

		if ( not dbgName.empty() )
		{
			dev.SetObjectName( BitCast<uint64_t>(_image), dbgName, VK_OBJECT_TYPE_IMAGE );
		}
		
		CHECK( desc.queueFamily == VK_QUEUE_FAMILY_IGNORED );	// not supported yet
		CHECK( desc.queueFamilyIndices.empty() or desc.queueFamilyIndices.size() >= 2 );

		_queueFamilyMask = Default;

		for (auto idx : desc.queueFamilyIndices) {
			_queueFamilyMask |= BitCast<EQueueFamily>(idx);
		}

		_aspectMask		= ChooseAspect( _desc.format );
		_defaultLayout	= ChooseDefaultLayout( _desc.usage, BitCast<VkImageLayout>(desc.defaultLayout) );
		_debugName		= dbgName;
		_onRelease		= std::move(onRelease);

		return true;
	}

/*
=================================================
	Destroy
=================================================
*/
	void VImage::Destroy (VResourceManager &resMngr)
	{
		EXLOCK( _drCheck );
		
		auto&	dev = resMngr.GetDevice();
		
		{
			SHAREDLOCK( _viewMapLock );
			for (auto& view : _viewMap) {
				dev.vkDestroyImageView( dev.GetVkDevice(), view.second, null );
			}
			_viewMap.clear();
		}

		if ( _desc.isExternal and _onRelease ) {
			_onRelease( BitCast<ImageVk_t>(_image) );
		}

		if ( not _desc.isExternal and _image ) {
			dev.vkDestroyImage( dev.GetVkDevice(), _image, null );
		}

		if ( _memoryId ) {
			resMngr.ReleaseResource( _memoryId.Release() );
		}
		
		_debugName.clear();

		_image				= VK_NULL_HANDLE;
		_memoryId			= Default;
		_desc				= Default;
		_aspectMask			= Zero;
		_defaultLayout		= Zero;
		_queueFamilyMask	= Default;
		_onRelease			= {};
	}
	
/*
=================================================
	GetView
=================================================
*/
	VkImageView  VImage::GetView (const VDevice &dev, const HashedImageViewDesc &desc) const
	{
		SHAREDLOCK( _drCheck );

		// find already created image view
		{
			SHAREDLOCK( _viewMapLock );

			auto	iter = _viewMap.find( desc );

			if ( iter != _viewMap.end() )
				return iter->second;
		}

		// create new image view
		EXLOCK( _viewMapLock );

		auto[iter, inserted] = _viewMap.insert({ desc, VK_NULL_HANDLE });

		if ( not inserted )
			return iter->second;	// other thread create view before
		
		CHECK_ERR( _CreateView( dev, desc, OUT iter->second ));

		return iter->second;
	}
	
/*
=================================================
	GetView
=================================================
*/
	VkImageView  VImage::GetView (const VDevice &dev, bool isDefault, INOUT ImageViewDesc &viewDesc) const
	{
		SHAREDLOCK( _drCheck );

		if ( isDefault )
			viewDesc = ImageViewDesc{ _desc };
		else
			viewDesc.Validate( _desc );

		return GetView( dev, viewDesc );
	}

/*
=================================================
	_CreateView
=================================================
*/
	bool VImage::_CreateView (const VDevice &dev, const HashedImageViewDesc &viewDesc, OUT VkImageView &outView) const
	{
		const VkComponentSwizzle	components[] = {
			VK_COMPONENT_SWIZZLE_IDENTITY,	// unknown
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A,
			VK_COMPONENT_SWIZZLE_ZERO,
			VK_COMPONENT_SWIZZLE_ONE
		};

		const auto&				desc		= viewDesc.Get();
		const uint4				swizzle		= Min( uint4(uint(CountOf(components)-1)), desc.swizzle.ToVec() );
		VkImageViewCreateInfo	view_info	= {};
		
		ASSERT( IsSupported( dev, desc ));

		view_info.sType			= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.pNext			= null;
		view_info.viewType		= VEnumCast( desc.viewType );
		view_info.flags			= 0;
		view_info.image			= Handle();
		view_info.format		= VEnumCast( desc.format );
		view_info.components	= { components[swizzle.x], components[swizzle.y], components[swizzle.z], components[swizzle.w] };

		view_info.subresourceRange.aspectMask		= VEnumCast( desc.aspectMask );
		view_info.subresourceRange.baseMipLevel		= desc.baseLevel.Get();
		view_info.subresourceRange.levelCount		= desc.levelCount;
		view_info.subresourceRange.baseArrayLayer	= desc.baseLayer.Get();
		view_info.subresourceRange.layerCount		= desc.layerCount;
		
		VK_CHECK( dev.vkCreateImageView( dev.GetVkDevice(), &view_info, null, OUT &outView ));
		return true;
	}
	
/*
=================================================
	IsReadOnly
=================================================
*/
	bool  VImage::IsReadOnly () const
	{
		SHAREDLOCK( _drCheck );
		return not AllBits( _desc.usage, EImageUsage::TransferDst | EImageUsage::ColorAttachment | EImageUsage::Storage |
										 EImageUsage::DepthStencilAttachment | EImageUsage::TransientAttachment |
										 EImageUsage::ColorAttachmentBlend | EImageUsage::StorageAtomic );
	}
	
/*
=================================================
	GetApiSpecificDescription
=================================================
*/
	VulkanImageDesc  VImage::GetApiSpecificDescription () const
	{
		VulkanImageDesc		desc;
		desc.image			= BitCast<ImageVk_t>( _image );
		desc.imageType		= BitCast<ImageTypeVk_t>( VEnumCast( _desc.imageType ));
		desc.flags			= BitCast<ImageFlagsVk_t>( VEnumCast( _desc.flags ));
		desc.usage			= BitCast<ImageUsageVk_t>( VEnumCast( _desc.usage ));
		desc.format			= BitCast<FormatVk_t>( VEnumCast( _desc.format ));
		desc.currentLayout	= BitCast<ImageLayoutVk_t>( _defaultLayout );	// TODO
		desc.defaultLayout	= desc.currentLayout;
		desc.samples		= BitCast<SampleCountFlagBitsVk_t>( VEnumCast( _desc.samples ));
		desc.dimension		= _desc.dimension;
		desc.arrayLayers	= _desc.arrayLayers.Get();
		desc.maxLevels		= _desc.maxLevel.Get();
		desc.queueFamily	= VK_QUEUE_FAMILY_IGNORED;
		//desc.queueFamilyIndices	// TODO
		return desc;
	}
	
/*
=================================================
	IsSupported
=================================================
*/
	bool  VImage::IsSupported (const VDevice &dev, const ImageDesc &desc, EMemoryType memType)
	{
		const bool		opt_tiling	= not AnyBits( memType, EMemoryType::HostRead | EMemoryType::HostWrite );
		const VkFormat	format		= VEnumCast( desc.format );

		// check available creation flags
		{
			VkImageCreateFlagBits	required	= VEnumCast( desc.flags );
			VkImageCreateFlagBits	available	= dev.GetFlags().imageCreateFlags;

			if ( not AllBits( available, required ))
				return false;
		}

		// check format features
		{
			VkFormatProperties	props = {};
			vkGetPhysicalDeviceFormatProperties( dev.GetVkPhysicalDevice(), format, OUT &props );
		
			const VkFormatFeatureFlags	available	= opt_tiling ? props.optimalTilingFeatures : props.linearTilingFeatures;
			VkFormatFeatureFlags		required	= 0;

			for (EImageUsage t = EImageUsage(1); t <= desc.usage; t = EImageUsage(uint(t) << 1))
			{
				if ( not AllBits( desc.usage, t ))
					continue;

				BEGIN_ENUM_CHECKS();
				switch ( t )
				{
					case EImageUsage::TransferSrc :				required |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT;	break;
					case EImageUsage::TransferDst :				required |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;	break;
					case EImageUsage::Sampled :					required |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;break;
					case EImageUsage::Storage :					required |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;					break;
					case EImageUsage::SampledMinMax :			required |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT;	break;
					case EImageUsage::StorageAtomic :			required |= VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT;				break;
					case EImageUsage::ColorAttachment :			required |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;					break;
					case EImageUsage::ColorAttachmentBlend :	required |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;			break;
					case EImageUsage::DepthStencilAttachment :	required |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;			break;
					case EImageUsage::TransientAttachment :		break;	// TODO
					case EImageUsage::InputAttachment :			break;
					case EImageUsage::ShadingRate :				if ( not dev.GetFeatures().shadingRateImageNV ) return false;		break;
					//case EImageUsage::FragmentDensityMap :	return false;	// not supported yet
					case EImageUsage::_Last :
					case EImageUsage::All :
					case EImageUsage::Transfer :
					case EImageUsage::Unknown :
					default :									ASSERT(false);	break;
				}
				END_ENUM_CHECKS();
			}

			if ( not AllBits( available, required ))
				return false;
		}

		// check image properties
		{
			VkImageFormatProperties	props = {};
			VK_CHECK( vkGetPhysicalDeviceImageFormatProperties( dev.GetVkPhysicalDevice(), format, VEnumCast( desc.imageType ),
																opt_tiling ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR,
																VEnumCast( desc.usage ), VEnumCast( desc.flags ), OUT &props ));

			if ( desc.dimension.x > props.maxExtent.width  or
			 	 desc.dimension.y > props.maxExtent.height or
				 desc.dimension.z > props.maxExtent.depth )
				return false;

			if ( desc.maxLevel.Get() > props.maxMipLevels )
				return false;

			if ( desc.arrayLayers.Get() > props.maxArrayLayers )
				return false;

			if ( not AllBits( props.sampleCounts, desc.samples.Get() ))
				return false;
		}

		return true;
	}
	
/*
=================================================
	IsSupported
=================================================
*/
	bool  VImage::IsSupported (const VDevice &dev, const ImageViewDesc &desc) const
	{
		SHAREDLOCK( _drCheck );
		
		if ( desc.viewType == EImage_CubeArray )
		{
			if ( not dev.GetProperties().features.imageCubeArray )
				return false;

			if ( _desc.imageType != EImageDim_2D or (_desc.imageType == EImageDim_3D and AllBits( _desc.flags, EImageFlags::Array2DCompatible)) )
				return false;

			if ( not AllBits( _desc.flags, EImageFlags::CubeCompatible ))
				return false;

			if ( desc.layerCount % 6 != 0 )
				return false;
		}

		if ( desc.viewType == EImage_Cube )
		{
			if ( not AllBits( _desc.flags, EImageFlags::CubeCompatible ))
				return false;

			if ( desc.layerCount != 6 )
				return false;
		}

		if ( _desc.imageType == EImageDim_3D and desc.viewType != EImage_3D )
		{
			if ( not AllBits( _desc.flags, EImageFlags::Array2DCompatible ))
				return false;
		}

		if ( desc.format != Default and desc.format != _desc.format )
		{
			auto&	required	= EPixelFormat_GetInfo( _desc.format );
			auto&	origin		= EPixelFormat_GetInfo( desc.format );
			bool	req_comp	= Any( required.blockSize > 1u );
			bool	orig_comp	= Any( origin.blockSize > 1u );
			
			if ( not AllBits( _desc.flags, EImageFlags::MutableFormat ))
				return false;

			// compressed to uncompressed
			if ( AllBits( _desc.flags, EImageFlags::BlockTexelViewCompatible ) and orig_comp and not req_comp )
			{
				if ( required.bitsPerBlock != origin.bitsPerBlock )
					return false;
			}
			else
			{
				if ( req_comp != orig_comp )
					return false;

				if ( Any( required.blockSize != origin.blockSize ))
					return false;

				if ( desc.aspectMask == EImageAspect::Stencil )
				{
					if ( required.bitsPerBlock2 != origin.bitsPerBlock2 )
						return false;
				}
				else
				{
					if ( required.bitsPerBlock != origin.bitsPerBlock )
						return false;
				}
			}
		}

		return true;
	}


}	// FG
