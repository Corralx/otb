#pragma once

#include <cstdint>
#include <cassert>
#include <cstring>
#include <memory>

// TODO(Corralx): Eventually add other formats and storages
enum class pixel_format : uint8_t
{
	U8 = 0,
	F32 = 1,
	U32 = 2
};

namespace detail
{

template<pixel_format>
struct format_to_pixel_info{};

#define DECLARE_PIXEL_INFO(_value, _type, _channels) \
template<> \
struct format_to_pixel_info<pixel_format::_value> \
{ \
	using type = _type;	\
\
	static const uint8_t channels = _channels; \
	static const size_t size = channels * sizeof(_type); \
}

DECLARE_PIXEL_INFO(U8, uint8_t, 1);
DECLARE_PIXEL_INFO(F32, float, 1);
DECLARE_PIXEL_INFO(U32, uint32_t, 1);

#undef DECLARE_PIXEL_INFO

}

#define GET_PIXEL_INFO(info) typename detail::format_to_pixel_info<F>::info

template<pixel_format F>
class image
{
	using Format = GET_PIXEL_INFO(type);

public:
	image() = delete;
	image(uint32_t width, uint32_t height) :
		_data(std::make_unique<Format[]>(width * height)), _width(width), _height(height)
	{
		assert(width > 0);
		assert(height > 0);
	}

	image(const image&) = delete;
	image(image&&) = default;

	image& operator=(const image&) = delete;
	image& operator=(image&&) = default;

	~image() = default;

	void reset(uint8_t val)
	{
		memset(_data.get(), val, memory());
	}

	pixel_format format() const
	{
		return F;
	}

	uint8_t channels() const
	{
		return GET_PIXEL_INFO(channels);
	}
	
	// NOTE(Corralx): Return the memory in bytes
	size_t memory() const
	{
		return GET_PIXEL_INFO(size) * _width * _height;
	}

	const Format* const raw() const
	{
		return _data.get();
	}

	Format* raw()
	{
		return _data.get();
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
	std::unique_ptr<Format[]> _data;
	uint32_t _width;
	uint32_t _height;
};

#undef GET_PIXEL_INFO
