#pragma once

#include <cstdint>
#include <cassert>

// TODO(Corralx): Eventually add other formats and storages
enum class image_format : uint8_t
{
	U8 = 0,
	F32 = 1
};

namespace detail
{

template<image_format>
struct format_to_pixel_info
{
};

template<>
struct format_to_pixel_info<image_format::U8>
{
	using type = uint8_t;

	static const uint8_t channels = 1;
	static const size_t size = channels * sizeof(type);
};

template<>
struct format_to_pixel_info<image_format::F32>
{
	using type = float;

	static const uint8_t channels = 1;
	static const size_t size = channels * sizeof(type);
};

}

template<image_format F>
class image
{
	using Format = typename detail::format_to_pixel_info<F>::type;

public:
	image() = delete;
	image(uint32_t width, uint32_t height) :
		_data(nullptr), _width(width), _height(height)
	{
		_data = new Format[width * height];

		assert(width > 0);
		assert(height > 0);
		assert(_data);
	}
	
	~image()
	{
		if (_data)
			delete[] _data;
	}

	image_format format() const
	{
		return F;
	}

	uint8_t channels() const
	{
		return typename detail::format_to_pixel_info<F>::channels;
	}

	size_t size() const
	{
		return typename detail::format_to_pixel_info<F>::size * _width * _height;
	}

	// TODO(Corralx): Should this be read-only?
	const Format* const raw() const
	{
		return _data;
	}

	uint32_t width() const
	{
		return _width;
	}

	uint32_t height() const
	{
		return _height;
	}

	Format& operator[](size_t index)
	{
		assert(index < _width * _height);
		return _data[index];
	}

private:
	Format* _data;
	uint32_t _width;
	uint32_t _height;
};
