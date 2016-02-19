/// @brief Include to copy textures or a subset of either textures. These operations are performed without memory allocations.
/// @file gli/copy.hpp

#pragma once

namespace gli
{
	// Copy a single texture image
	template <typename texture_type>
	void copy(
		texture_type const& TextureSrc, typename texture_type::size_type BaseLevelSrc,
		texture_type& TextureDst, typename texture_type::size_type BaseLevelDst);

	// Copy multiple texture images
	template <typename texture_type>
	void copy(
		texture_type const& TextureSrc, typename texture_type::size_type BaseLevelSrc,
		texture_type& TextureDst, typename texture_type::size_type BaseLevelDst,
		typename texture_type::size_type LevelCount);
}//namespace gli

#include "./core/copy.inl"
