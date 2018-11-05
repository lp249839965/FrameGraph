// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "framegraph/Public/LowLevel/IDs.h"
#include "framegraph/Public/LowLevel/ImageDesc.h"
#include "framegraph/Public/LowLevel/Pipeline.h"
#include <atomic>

namespace FG
{

	//
	// Pipeline Resources
	//

	struct PipelineResources final
	{
		friend class PipelineResourcesInitializer;

	// types
	public:
		using Self	= PipelineResources;

		struct Buffer
		{
			BindingIndex			index;
			EResourceState			state;
			RawBufferID				bufferId;
			BytesU					offset;
			BytesU					size;

			ND_ bool  operator == (const Buffer &rhs) const {
				return	state		== rhs.state	and 
						bufferId	== rhs.bufferId	and
						offset		== rhs.offset	and
						size		== rhs.size;
			}
		};

		struct Image
		{
			BindingIndex			index;
			EResourceState			state;
			RawImageID				imageId;
			Optional<ImageViewDesc>	desc;

			ND_ bool  operator == (const Image &rhs) const {
				return	state	== rhs.state	and
						imageId	== rhs.imageId	and
						desc	== rhs.desc;
			}
		};

		struct Texture final : Image
		{
			RawSamplerID			samplerId;

			ND_ bool  operator == (const Texture &rhs) const {
				return	Image::operator== (rhs) and
						samplerId == rhs.samplerId;
			}
		};

		struct SubpassInput final : Image
		{
			uint					attachmentIndex	= ~0u;

			ND_ bool  operator == (const SubpassInput &rhs) const {
				return	Image::operator== (rhs) and
						attachmentIndex == rhs.attachmentIndex;
			}
		};

		struct Sampler
		{
			BindingIndex			index;
			RawSamplerID			samplerId;

			ND_ bool  operator == (const Sampler &rhs) const {
				return	samplerId == rhs.samplerId;
			}
		};

		using Resource_t	= Union< std::monostate, Buffer, Image, Texture, Sampler, SubpassInput >;

		struct ResourceInfo
		{
			HashVal			hash;
			Resource_t		res;
		};

		using ResourceSet_t		= Array< ResourceInfo >;
		using DynamicOffsets_t	= FixedArray< uint, FG_MaxBufferDynamicOffsets >;
		using UniformMapPtr		= PipelineDescription::UniformMapPtr;
		using CachedID			= std::atomic< uint64_t >;

		STATIC_ASSERT( sizeof(CachedID) == sizeof(RawPipelineResourcesID) );


	// variables
	private:
		RawDescriptorSetLayoutID	_layoutId;
		UniformMapPtr				_uniforms;
		ResourceSet_t				_resources;
		DynamicOffsets_t			_dynamicOffsets;

		mutable CachedID			_cachedId	{ ~0ull };


	// methods
	public:
		PipelineResources () noexcept {}
		PipelineResources (PipelineResources &&) = default;

		Self&	BindImage (const UniformID &id, const ImageID &image) noexcept;
		Self&	BindImage (const UniformID &id, const ImageID &image, const ImageViewDesc &desc) noexcept;

		Self&	BindTexture (const UniformID &id, const ImageID &image, const SamplerID &sampler) noexcept;
		Self&	BindTexture (const UniformID &id, const ImageID &image, const SamplerID &sampler, const ImageViewDesc &desc) noexcept;

		Self&	BindSampler (const UniformID &id, const SamplerID &sampler) noexcept;

		Self&	BindBuffer (const UniformID &id, const BufferID &buffer) noexcept;
		Self&	BindBuffer (const UniformID &id, const BufferID &buffer, BytesU offset, BytesU size) noexcept;
		Self&	SetBufferBase (const UniformID &id, BytesU offset) noexcept;

		ND_ bool  HasImage (const UniformID &id)	const noexcept;
		ND_ bool  HasSampler (const UniformID &id)	const noexcept;
		ND_ bool  HasTexture (const UniformID &id)	const noexcept;
		ND_ bool  HasBuffer (const UniformID &id)	const noexcept;

		ND_ RawDescriptorSetLayoutID const&	GetLayout ()			const	{ return _layoutId; }
		ND_ ResourceSet_t const&			GetData ()				const	{ return _resources; }
		ND_ ArrayView< uint >				GetDynamicOffsets ()	const	{ return _dynamicOffsets; }


	private:
		void _BindUniformBuffer (INOUT ResourceInfo &res, const PipelineDescription::UniformBuffer &un, const RawBufferID &buffer, BytesU offset, BytesU size);
		void _BindStorageBuffer (INOUT ResourceInfo &res, const PipelineDescription::StorageBuffer &un, const RawBufferID &buffer, BytesU offset, BytesU size);

		void _SetCachedID (const RawPipelineResourcesID &id)	const	{ _cachedId.store( BitCast<uint64_t>(id), std::memory_order_release ); }
		void _ResetCachedID ()									const	{ _SetCachedID( RawPipelineResourcesID() ); }

		ND_ RawPipelineResourcesID const&	_GetCachedID ()		const	{ return BitCast<RawPipelineResourcesID>( _cachedId.load( std::memory_order_acquire )); }
	};


	using PipelineResourceSet	= FixedMap< DescriptorSetID, PipelineResources const*, FG_MaxDescriptorSets >;

	

/*
=================================================
	BindImage
=================================================
*/
	inline PipelineResources&  PipelineResources::BindImage (const UniformID &id, const ImageID &image) noexcept
	{
		ASSERT( HasImage( id ) );
		auto	un		= _uniforms->find( id );
		auto&	curr	= _resources[ un->second.index.Unique() ];
		auto&	img		= std::get<Image>( curr.res );

		if ( img.imageId != image.Get() or img.desc.has_value() )
			_ResetCachedID();

		img.imageId	= image.Get();
		img.desc.reset();

		curr.hash = HashOf( img.imageId );
		return *this;
	}
	
/*
=================================================
	BindImage
=================================================
*/
	inline PipelineResources&  PipelineResources::BindImage (const UniformID &id, const ImageID &image, const ImageViewDesc &desc) noexcept
	{
		ASSERT( HasImage( id ) );
		auto	un		= _uniforms->find( id );
		auto&	curr	= _resources[ un->second.index.Unique() ];
		auto&	img		= std::get<Image>( curr.res );

		if ( img.imageId != image.Get() or not (img.desc == desc) )
			_ResetCachedID();

		img.imageId	= image.Get();
		img.desc	= desc;

		curr.hash = HashOf( img.imageId ) + HashOf( img.desc.value() );
		return *this;
	}

/*
=================================================
	HasImage
=================================================
*/
	inline bool  PipelineResources::HasImage (const UniformID &id) const noexcept
	{
		if ( _uniforms ) {
			auto	un = _uniforms->find( id );
			return (un != _uniforms->end() and (std::holds_alternative<PipelineDescription::Image>( un->second.data ) or
												std::holds_alternative<PipelineDescription::SubpassInput>( un->second.data )) );
		}
		return false;
	}

/*
=================================================
	BindTexture
=================================================
*/
	inline PipelineResources&  PipelineResources::BindTexture (const UniformID &id, const ImageID &image, const SamplerID &sampler) noexcept
	{
		ASSERT( HasTexture( id ) );
		auto	un		= _uniforms->find( id );
		auto&	curr	= _resources[ un->second.index.Unique() ];
		auto&	tex		= std::get<Texture>( curr.res );
		
		if ( tex.imageId != image.Get() or tex.samplerId != sampler.Get() or tex.desc.has_value() )
			_ResetCachedID();

		tex.imageId		= image.Get();
		tex.samplerId	= sampler.Get();
		tex.desc.reset();

		curr.hash = HashOf( tex.imageId ) + HashOf( tex.samplerId );
		return *this;
	}
	
/*
=================================================
	BindTexture
=================================================
*/
	inline PipelineResources&  PipelineResources::BindTexture (const UniformID &id, const ImageID &image, const SamplerID &sampler, const ImageViewDesc &desc) noexcept
	{
		ASSERT( HasTexture( id ) );
		auto	un		= _uniforms->find( id );
		auto&	curr	= _resources[ un->second.index.Unique() ];
		auto&	tex		= std::get<Texture>( curr.res );
		
		if ( tex.imageId != image.Get() or tex.samplerId != sampler.Get() or not (tex.desc == desc) )
			_ResetCachedID();

		tex.imageId		= image.Get();
		tex.samplerId	= sampler.Get();
		tex.desc		= desc;
		
		curr.hash = HashOf( tex.imageId ) + HashOf( tex.samplerId ) + HashOf( tex.desc.value() );
		return *this;
	}
	
/*
=================================================
	HasTexture
=================================================
*/
	inline bool  PipelineResources::HasTexture (const UniformID &id) const noexcept
	{
		if ( _uniforms ) {
			auto	un = _uniforms->find( id );
			return (un != _uniforms->end() and std::holds_alternative<PipelineDescription::Texture>( un->second.data ));
		}
		return false;
	}

/*
=================================================
	BindSampler
=================================================
*/
	inline PipelineResources&  PipelineResources::BindSampler (const UniformID &id, const SamplerID &sampler) noexcept
	{
		ASSERT( HasSampler( id ) );
		auto	un		= _uniforms->find( id );
		auto&	curr	= _resources[ un->second.index.Unique() ];
		auto&	samp	= std::get<Sampler>( curr.res );

		if ( samp.samplerId != sampler.Get() )
			_ResetCachedID();

		samp.samplerId = sampler.Get();

		curr.hash = HashOf( samp.samplerId );
		return *this;
	}
	
/*
=================================================
	HasSampler
=================================================
*/
	inline bool  PipelineResources::HasSampler (const UniformID &id) const noexcept
	{
		if ( _uniforms ) {
			auto	un = _uniforms->find( id );
			return (un != _uniforms->end() and std::holds_alternative<PipelineDescription::Sampler>( un->second.data ));
		}
		return false;
	}

/*
=================================================
	BindBuffer
=================================================
*/
	inline PipelineResources&  PipelineResources::BindBuffer (const UniformID &id, const BufferID &buffer) noexcept
	{
		ASSERT( HasBuffer( id ) );
		auto	un = _uniforms->find( id );

		if ( auto* ubuf = std::get_if<PipelineDescription::UniformBuffer>( &un->second.data ) )
		{
			_BindUniformBuffer( INOUT _resources[ un->second.index.Unique() ], *ubuf, buffer.Get(), 0_b, ~0_b );
		}
		else
		if ( auto* sbuf = std::get_if<PipelineDescription::StorageBuffer>( &un->second.data ) )
		{
			_BindStorageBuffer( INOUT _resources[ un->second.index.Unique() ], *sbuf, buffer.Get(), 0_b, ~0_b );
		}
		return *this;
	}
	
/*
=================================================
	BindBuffer
=================================================
*/
	inline PipelineResources&  PipelineResources::BindBuffer (const UniformID &id, const BufferID &buffer, BytesU offset, BytesU size) noexcept
	{
		ASSERT( HasBuffer( id ) );
		auto	un = _uniforms->find( id );
		
		if ( auto* ubuf = std::get_if<PipelineDescription::UniformBuffer>( &un->second.data ) )
		{
			_BindUniformBuffer( INOUT _resources[ un->second.index.Unique() ], *ubuf, buffer.Get(), offset, size );
		}
		else
		if ( auto* sbuf = std::get_if<PipelineDescription::StorageBuffer>( &un->second.data ) )
		{
			_BindStorageBuffer( INOUT _resources[ un->second.index.Unique() ], *sbuf, buffer.Get(), offset, size );
		}
		return *this;
	}
/*
=================================================
	_BindUniformBuffer
=================================================
*/
	inline void PipelineResources::_BindUniformBuffer (INOUT ResourceInfo &curr, const PipelineDescription::UniformBuffer &un, const RawBufferID &id, BytesU offset, BytesU size)
	{
		auto&	buf = std::get<Buffer>( curr.res );
		ASSERT( (un.size == size) or (size == ~0_b) );
		
		bool	changed = (buf.bufferId != id);
		
		if ( un.dynamicOffsetIndex == PipelineDescription::STATIC_OFFSET )
		{
			changed		|= (buf.offset != offset);
			buf.offset	 = offset;
		}
		else
		{
			ASSERT( offset < std::numeric_limits<uint>::max() );
			_dynamicOffsets[un.dynamicOffsetIndex] = uint(offset);
		}

		if ( changed )
			_ResetCachedID();

		buf.bufferId = id;

		curr.hash = HashOf( buf.bufferId ) + HashOf( buf.size ) + HashOf( buf.offset );
	}
	
/*
=================================================
	_BindStorageBuffer
=================================================
*/
	inline void PipelineResources::_BindStorageBuffer (INOUT ResourceInfo &curr, const PipelineDescription::StorageBuffer &un, const RawBufferID &id, BytesU offset, BytesU size)
	{
		auto&	buf = std::get<Buffer>( curr.res );
		ASSERT( size == ~0_b or ((size >= un.staticSize) and (un.arrayStride == 0 or (size - un.staticSize) % un.arrayStride == 0)) );
		
		bool	changed = (buf.bufferId	!= id or buf.size != size);
		
		if ( un.dynamicOffsetIndex == PipelineDescription::STATIC_OFFSET )
		{
			changed		|= (buf.offset != offset);
			buf.offset	 = offset;
		}
		else
		{
			ASSERT( offset < std::numeric_limits<uint>::max() );
			_dynamicOffsets[un.dynamicOffsetIndex] = uint(offset);
		}
		
		if ( changed )
			_ResetCachedID();

		buf.bufferId	= id;
		buf.size		= size;

		curr.hash = HashOf( buf.bufferId ) + HashOf( buf.size ) + HashOf( buf.offset );
	}
	
/*
=================================================
	SetBufferBase
=================================================
*/
	inline PipelineResources&  PipelineResources::SetBufferBase (const UniformID &id, BytesU offset) noexcept
	{
		ASSERT( HasBuffer( id ) );
		auto	un		= _uniforms->find( id );
		auto&	curr	= _resources[ un->second.index.Unique() ];
		auto&	buf		= std::get<Buffer>( curr.res );

		DEBUG_ONLY(
			if ( auto* ubuf = std::get_if<PipelineDescription::UniformBuffer>( &un->second.data ) )
				ASSERT( ubuf->dynamicOffsetIndex != PipelineDescription::STATIC_OFFSET )
			else
			if ( auto* sbuf = std::get_if<PipelineDescription::StorageBuffer>( &un->second.data ) )
				ASSERT( sbuf->dynamicOffsetIndex != PipelineDescription::STATIC_OFFSET );
		)

		if ( buf.offset != offset )
			_ResetCachedID();

		buf.offset = offset;
		
		curr.hash = HashOf( buf.bufferId ) + HashOf( buf.size ) + HashOf( buf.offset );
	}

/*
=================================================
	HasBuffer
=================================================
*/
	inline bool  PipelineResources::HasBuffer (const UniformID &id) const noexcept
	{
		if ( _uniforms ) {
			auto	un = _uniforms->find( id );
			return (un != _uniforms->end() and (std::holds_alternative<PipelineDescription::UniformBuffer>( un->second.data ) or
												std::holds_alternative<PipelineDescription::StorageBuffer>( un->second.data )) );
		}
		return false;
	}


}	// FG