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
struct format_to_storage_type
{
};

template<>
struct format_to_storage_type<image_format::U8>
{
	using type = uint8_t;
};

template<>
struct format_to_storage_type<image_format::F32>
{
	using type = float;
};

}

template<image_format F>
class image
{
	using Format = typename detail::format_to_storage_type<F>::type;

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

	// TODO(Corralx): Do we want this? Should it be read-only?
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
