#pragma once

#include <cstdint>
#include <cassert>
#include <cstring>

// TODO(Corralx): Eventually add other formats and storages
enum class image_format : uint8_t
{
	U8 = 0,
	F32 = 1,
	U32 = 2
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
	static const size_t size = channels * sizeof(uint8_t);
};

template<>
struct format_to_pixel_info<image_format::F32>
{
	using type = float;

	static const uint8_t channels = 1;
	static const size_t size = channels * sizeof(float);
};

template<>
struct format_to_pixel_info<image_format::U32>
{
	using type = uint32_t;

	static const uint8_t channels = 1;
	static const size_t size = channels * sizeof(uint32_t);
};

}

#define GET_PIXEL_INFO(info) typename detail::format_to_pixel_info<F>::info

template<image_format F>
class image
{
	using Format = GET_PIXEL_INFO(type);

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
		assert(_data);
		delete[] _data;
	}

	void initialize(uint8_t val)
	{
		memset(_data, val, size());
	}

	image_format format() const
	{
		return F;
	}

	uint8_t channels() const
	{
		return GET_PIXEL_INFO(channels);
	}
	
	// NOTE(Corralx): Return the size in bytes
	size_t size() const
	{
		return GET_PIXEL_INFO(size) * _width * _height;
	}

	const Format* const raw() const
	{
		return _data;
	}

	Format* raw()
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

	const Format& operator[](size_t index) const
	{
		assert(index < _width * _height);
		return _data[index];
	}

private:
	Format* _data;
	uint32_t _width;
	uint32_t _height;
};

#undef GET_PIXEL_INFO
