#include "../type.hpp"
#include <cstring>

namespace gli
{
	template <typename texture_type>
	void copy
	(
		texture_type const& TextureSrc,
		typename texture_type::size_type LayerSrc,
		typename texture_type::size_type FaceSrc,
		typename texture_type::size_type LevelSrc,
		texture_type& TextureDst,
		typename texture_type::size_type LayerDst,
		typename texture_type::size_type FaceDst,
		typename texture_type::size_type LevelDst
	)
	{
		GLI_ASSERT(TextureDst.size(LevelDst) == TextureSrc.size(LevelSrc));
		GLI_ASSERT(LayerSrc < TextureSrc.layers());
		GLI_ASSERT(LayerDst < TextureDst.layers());
		GLI_ASSERT(FaceSrc < TextureSrc.faces());
		GLI_ASSERT(FaceDst < TextureDst.faces());
		GLI_ASSERT(LevelSrc < TextureSrc.levels());
		GLI_ASSERT(LevelDst < TextureDst.levels());
		
		memcpy(
			TextureDst.data(LayerDst, FaceDst, LevelDst),
			TextureSrc.data(LayerSrc, FaceSrc, LevelSrc),
			TextureDst.size(LevelDst));
	}

	template <typename texture_type>
	void copy
	(
		texture_type const& TextureSrc, typename texture_type::size_type BaseLevelSrc,
		texture_type& TextureDst, typename texture_type::size_type BaseLevelDst,
		typename texture_type::size_type LevelCount
	)
	{
		GLI_ASSERT(TextureDst.size(BaseLevelSrc) == TextureSrc.size(BaseLevelDst));
		GLI_ASSERT(BaseLevelSrc < TextureSrc.levels());
		GLI_ASSERT(BaseLevelSrc + LevelCount <= TextureSrc.levels());
		GLI_ASSERT(BaseLevelDst < TextureDst.levels());
		GLI_ASSERT(BaseLevelDst + LevelCount <= TextureDst.levels());
		
		for(typename texture_type::size_type LayerIndex = 0, LayerCount = TextureSrc.layers(); LayerIndex < LayerCount; ++LayerIndex)
		for(typename texture_type::size_type FaceIndex = 0, FaceCount = TextureSrc.faces(); FaceIndex < FaceCount; ++FaceIndex)
		for(typename texture_type::size_type LevelIndex = 0; LevelIndex < LevelCount; ++LevelIndex)
		{
			memcpy(
				TextureDst.data(LayerIndex, FaceIndex, BaseLevelDst + LevelIndex),
				TextureSrc.data(LayerIndex, FaceIndex, BaseLevelSrc + LevelIndex),
				TextureDst.size(BaseLevelDst + LevelIndex));
		}
	}
	
	template <typename texture_type>
	void copy
	(
		texture_type const& TextureSrc, typename texture_type::size_type BaseLevelSrc,
		texture_type& TextureDst, typename texture_type::size_type BaseLevelDst
	)
	{
		copy(TextureSrc, BaseLevelSrc, TextureDst, BaseLevelDst, 1);
	}

	template <typename texture_type>
	void copy_layer
	(
		texture_type const& TextureSrc, typename texture_type::size_type BaseLayerSrc,
		texture_type& TextureDst, typename texture_type::size_type BaseLayerDst,
		typename texture_type::size_type LayerCount
	)
	{
		GLI_ASSERT(TextureSrc.extent() == TextureDst.extent());
		GLI_ASSERT(TextureSrc.faces() == TextureDst.faces());
		GLI_ASSERT(TextureSrc.levels() == TextureDst.levels());
		GLI_ASSERT(BaseLayerSrc < TextureSrc.layers());
		GLI_ASSERT(BaseLayerSrc + LayerCount <= TextureSrc.layers());
		GLI_ASSERT(BaseLayerDst < TextureDst.layers());
		GLI_ASSERT(BaseLayerDst + LayerCount <= TextureDst.layers());
		
		for(typename texture_type::size_type LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
		for(typename texture_type::size_type FaceIndex = 0, FaceCount = TextureSrc.faces(); FaceIndex < FaceCount; ++FaceIndex)
		for(typename texture_type::size_type LevelIndex = 0, LevelCount = TextureSrc.levels(); LevelIndex < LevelCount; ++LevelIndex)
		{
			memcpy(
				TextureDst.data(BaseLayerDst + LayerIndex, FaceIndex, LevelIndex),
				TextureSrc.data(BaseLayerSrc + LayerIndex, FaceIndex, LevelIndex),
				TextureDst.size(LevelIndex));
		}
	}
}//namespace gli
