#pragma once

#include <cstdint>
#include <vector>
#include <cassert>
#include <limits>

#include "SDL2/SDL.h"
#include "GL/gl3w.h"

using buffer_handle = uint32_t;

enum class buffer_type : uint32_t
{
	VERTEX = GL_ARRAY_BUFFER,
	INDEX = GL_ELEMENT_ARRAY_BUFFER,
	UNIFORM = GL_UNIFORM_BUFFER
};

enum class buffer_usage : uint32_t
{
	STATIC = GL_STATIC_DRAW,
	DYNAMIC = GL_DYNAMIC_DRAW
};

extern SDL_Window* window;

// TODO(Corralx): Figure out where to save the indices provided by the manager (maybe mesh/material?)
class buffer_manager
{
public:
	buffer_manager()
	{
		// NOTE(Corralx): Create a context that can share the resources with the main context
		// Needs an active context when constructed!
		auto old_context = SDL_GL_GetCurrentContext();
		_context = SDL_GL_CreateContext(window);
		SDL_GL_MakeCurrent(window, old_context);
	}

	~buffer_manager()
	{
		// TODO(Corralx): Remove every active gl resources?
		SDL_GL_DeleteContext(_context);
	}

	buffer_manager(const buffer_manager&) = delete;
	buffer_manager(buffer_manager&&) = default;

	buffer_manager& operator=(const buffer_manager&) = delete;
	buffer_manager& operator=(buffer_manager&&) = default;

	// NOTE(Corralx): We restore the original context if any to call
	// create_buffer(...) either synchronously or asynchronously
	template<typename T>
	size_t create_buffer(buffer_type type, buffer_usage usage, const std::vector<T>& data)
	{
		auto old_context = SDL_GL_GetCurrentContext();
		SDL_GL_MakeCurrent(window, _context);

		uint32_t handle;
		glGenBuffers(1, &handle);
		glBindBuffer((uint32_t)type, handle);
		glBufferData((uint32_t)type, sizeof(T) * data.size(), data.data(), (uint32_t)usage);

		size_t handle_index = _buffers.size();
		_buffers.push_back(handle);
		
		if (old_context)
			SDL_GL_MakeCurrent(window, old_context);
		assert(glGetError() == GL_NO_ERROR);
		return handle_index;
	}

	// NOTE(Corralx): The indices are never reused for simplicity
	void release_buffer(size_t index)
	{
		assert(index < _buffers.size());
		buffer_handle handle = _buffers[index];
		glDeleteBuffers(1, &handle);
		_buffers[index] = std::numeric_limits<buffer_handle>::max();
	}

	buffer_handle get(size_t index) const
	{
		return _buffers.at(index);
	}

	buffer_handle operator[](size_t index) const
	{
		return _buffers[index];
	}

private:
	SDL_GLContext _context;
	std::vector<buffer_handle> _buffers;
};
