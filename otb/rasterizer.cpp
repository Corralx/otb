#include "rasterizer.hpp"
#include "mesh.hpp"
#include "utils.hpp"

#include "GL/gl3w.h"
#include "SDL2/SDL.h"
#include "glm/glm.hpp"

using image_u32 = image<pixel_format::U32>;

extern SDL_Window* window;

// NOTE(Corralx): We are on a separate thread so we use a different gl context
static void rasterize_hardware_helper(const mesh_t& mesh, image_u32& image, std::promise<void> promise)
{
	static auto context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window, context);

	// Texture
	uint32_t texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, image.width(), image.height(), 0, GL_RED_INTEGER, GL_UNSIGNED_INT, image.raw());

	// Framebuffer
	uint32_t framebuffer;
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

	assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	// VAO
	uint32_t vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// Vertices
	const auto& vertex_data = mesh.texture_coords();
	uint32_t vertex_buffer;
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(float) * vertex_data.size(), vertex_data.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	// Indices
	const auto& index_data = mesh.faces();
	uint32_t index_buffer;
	glGenBuffers(1, &index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * sizeof(uint32_t) * index_data.size(), index_data.data(), GL_STATIC_DRAW);

	// Program
	const char* vs_source =
		"#version 330 core\n \
		\n \
		layout(location = 0) in vec2 position;\n \
		\n \
		void main()\n \
		{\n \
			gl_Position = vec4(position * 2.0 - 1.0, 0.5, 1.0);\n \
		}\n \
		";

	const char* fs_source =
		"#version 330 core\n \
		\n \
		layout(location = 0) out uint out_color;\n \
		\n \
		void main()\n \
		{\n \
			out_color = uint(gl_PrimitiveID);\n \
		}\n \
		";

	uint32_t vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vs_source, nullptr);
	glCompileShader(vs);

	uint32_t fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fs_source, nullptr);
	glCompileShader(fs);

	uint32_t program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	assert(glGetError() == GL_NO_ERROR);

	glUseProgram(program);
	glViewport(0, 0, image.width(), image.height());

	// Draw
	glDrawElements(GL_TRIANGLES, (uint32_t)index_data.size() * 3, GL_UNSIGNED_INT, nullptr);

	// Copy back the data
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, image.raw());

	// Delete program
	glDeleteShader(vs);
	glDeleteShader(fs);
	glDeleteProgram(program);

	// Delete buffers
	glDeleteBuffers(1, &index_buffer);
	glDeleteBuffers(1, &vertex_buffer);
	glDeleteVertexArrays(1, &vao);

	// Delete framebuffer
	glDeleteTextures(1, &texture);
	glDeleteFramebuffers(1, &framebuffer);
	assert(glGetError() == GL_NO_ERROR);

	promise.set_value();
}

std::future<void> rasterize_triangle_hardware(const mesh_t& mesh, image_u32& image)
{
	return async_apply(rasterize_hardware_helper, std::ref(mesh), std::ref(image));
}

// TODO(Corralx): Reimplement software version (see code below)
static void rasterize_software_helper(const mesh_t& mesh, image_u32& image, uint8_t supersampling, std::promise<void> promise)
{
	mesh;
	image;
	supersampling;

	promise.set_value();
}

std::future<void> rasterize_triangle_software(const mesh_t & mesh, image_u32& image, uint8_t supersampling)
{
	return async_apply(rasterize_software_helper, std::ref(mesh), std::ref(image), supersampling);
}

/*
struct edge_t
{
	edge_t() = delete;
	edge_t(const glm::vec2& _v0, const glm::vec2& _v1) : v0(_v0), v1(_v1)
	{
		// Ordering vertices so that v0 is always the lower end
		if (_v0.y > _v1.y)
		{
			v0 = _v1;
			v1 = _v0;
		}
	}

	glm::vec2 v0;
	glm::vec2 v1;
};

struct span_t
{
	span_t() = delete;
	span_t(uint32_t _x0, uint32_t _x1) : x0(_x0), x1(_x1)
	{
		// Ordering vertices so that x0 is always the left end
		if (_x0 > _x1)
		{
			x0 = _x1;
			x1 = _x0;
		}
	}

	uint32_t x0;
	uint32_t x1;
};

uint32_t* indices_map;
{
	const uint8_t ssaa_scale = 2;
	const uint32_t width = occlusion_map_width * ssaa_scale;
	const uint32_t height = occlusion_map_height * ssaa_scale;

	const tinyobj::mesh_t& mesh = shapes[mesh_index].mesh;
	const size_t triangle_num = mesh.indices.size() / 3;

	// Allocate supersampled index map and set an invalid value used as sentinel
	uint32_t* index_map = new uint32_t[width * height];
	memset(index_map, 255, width * height * sizeof(uint32_t));

	// For each triangle, lookup the pixel it's covering in the occlusion map through triangle scan conversion
	// http://joshbeam.com/articles/triangle_rasterization/
	// http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html
	for (uint32_t tris_index = 0; tris_index < triangle_num; ++tris_index)
	{
		// Vertices index of current triangle
		const uint32_t v0_index = mesh.indices[tris_index * 3 + 0];
		const uint32_t v1_index = mesh.indices[tris_index * 3 + 1];
		const uint32_t v2_index = mesh.indices[tris_index * 3 + 2];

		// UV coordinates of current triangle
		const glm::vec2 v0_coord{ mesh.texcoords[v0_index * 2 + 0],
			mesh.texcoords[v0_index * 2 + 1] };
		const glm::vec2 v1_coord{ mesh.texcoords[v1_index * 2 + 0],
			mesh.texcoords[v1_index * 2 + 1] };
		const glm::vec2 v2_coord{ mesh.texcoords[v2_index * 2 + 0] ,
			mesh.texcoords[v2_index * 2 + 1] };

		// Pixel coordinates pf current triangle
		const glm::vec2 v0_pixel{ v0_coord.x * width + 0.5,
			v0_coord.y * height + 0.5 };
		const glm::vec2 v1_pixel{ v1_coord.x * width + 0.5,
			v1_coord.y * height + 0.5 };
		const glm::vec2 v2_pixel{ v2_coord.x * width + 0.5,
			v2_coord.y * height + 0.5 };

		// Current triangle edges
		const edge_t edges[3] =
		{
			{ v0_pixel, v1_pixel },
			{ v1_pixel, v2_pixel },
			{ v2_pixel, v0_pixel }
		};

		// Find the longer edge vertically
		uint32_t long_edge_index = 0;
		{
			float max_length = 0;
			for (uint32_t edge_index = 0; edge_index < 3; ++edge_index)
			{
				float length = edges[edge_index].v1.y - edges[edge_index].v0.y;
				if (length > max_length)
				{
					max_length = length;
					long_edge_index = edge_index;
				}
			}
		}

		const edge_t long_edge = edges[long_edge_index];
		const float long_edge_x_extent = long_edge.v1.x - long_edge.v0.x;
		const float long_edge_y_extent = long_edge.v1.y - long_edge.v0.y;

		// For each short edge, get all the pixel between it and the long edge
		for (uint32_t short_offset = 0; short_offset < 2; ++short_offset)
		{
			const uint32_t short_edge_index = (long_edge_index + 1 + short_offset) % 3;
			const edge_t short_edge = edges[short_edge_index];

			// Split the long segment to just account for the part as high as the short one
			const float x0_factor = (short_edge.v0.y - long_edge.v0.y) / long_edge_y_extent;
			const float x1_factor = (short_edge.v1.y - long_edge.v0.y) / long_edge_y_extent;
			const float long_segment_x0 = long_edge.v0.x + long_edge_x_extent * x0_factor;
			const float long_segment_x1 = long_edge.v0.x + long_edge_x_extent * x1_factor;
			const edge_t long_segment
			{
				{ long_segment_x0, short_edge.v0.y },
				{ long_segment_x1, short_edge.v1.y }
			};

			// Check if an edge is parallel to the X axis because it doesn't contribute
			// Both edges are actually checked to account for degenerates triangle
			const float long_edge_y_diff = long_segment.v1.y - long_segment.v0.y;
			if (std::abs(long_edge_y_diff) < .0001f)
				continue;

			const float short_edge_y_diff = short_edge.v1.y - short_edge.v0.y;
			if (short_edge_y_diff < .0001f)
				continue;

			const float long_edge_x_diff = long_segment.v1.x - long_segment.v0.x;
			const float short_edge_x_diff = short_edge.v1.x - short_edge.v0.x;

			float long_interp_factor = (short_edge.v0.y - long_segment.v0.y) / long_edge_y_diff;
			const float long_interp_step = 1.f / long_edge_y_diff;
			float short_interp_factor = .0f;
			const float short_interp_step = 1.f / short_edge_y_diff;

			// For each row the triangle is covering
			const uint32_t starting_row = static_cast<uint32_t>(long_segment.v0.y + .5f);
			const uint32_t ending_row = std::min(static_cast<uint32_t>(long_segment.v1.y + .5f), height);
			for (uint32_t row = starting_row; row < ending_row; ++row)
			{
				const float x0 = short_edge.v0.x + (short_edge_x_diff * short_interp_factor);
				const float x1 = long_segment.v0.x + (long_edge_x_diff * long_interp_factor);

				span_t span{ static_cast<uint32_t>(x0 + .5f), std::min(static_cast<uint32_t>(x1 + .5f), width) };

				// If the span is degenerate there is nothing to do
				uint32_t span_x_diff = span.x1 - span.x0;
				if (span_x_diff != 0)
				{
					// Save the current triangle index in the current pixel position
					for (uint32_t column = span.x0; column < span.x1; ++column)
						index_map[row * width + column] = tris_index;
				}

				short_interp_factor += short_interp_step;
				long_interp_factor += long_interp_step;
			}
		}
	}

	// Now downsample the result into the definitive indices map
	indices_map = new uint32_t[occlusion_map_width * occlusion_map_height];
	for (uint32_t i = 0; i < occlusion_map_height; ++i)
	{
		for (uint32_t j = 0; j < occlusion_map_width; ++j)
		{
			uint32_t samples[ssaa_scale * ssaa_scale] = {};

			// Collect all samples associated with this pixel
			for (uint32_t i_sample = 0; i_sample < ssaa_scale; ++i_sample)
			{
				for (uint32_t j_sample = 0; j_sample < ssaa_scale; ++j_sample)
				{
					const uint32_t dst_index = i_sample * ssaa_scale + j_sample;
					const uint32_t src_index = (i * ssaa_scale + i_sample) * width + j * ssaa_scale + j_sample;
					samples[dst_index] = index_map[src_index];
				}
			}

			uint32_t best_sample = std::numeric_limits<uint32_t>::max();
			ptrdiff_t sample_count = 0;
			uint32_t valid_sample = std::numeric_limits<uint32_t>::max();

			// Look for the samples with the higher count
			for (uint32_t sample_idx = 0; sample_idx < ssaa_scale * ssaa_scale; ++sample_idx)
			{
				ptrdiff_t count = std::count(std::begin(samples), std::end(samples), samples[sample_idx]);
				if (count > sample_count)
				{
					sample_count = count;
					best_sample = samples[sample_idx];
				}

				// Saving a valid sample for later, in case we don't find a sample which dominates
				if (samples[sample_idx] != std::numeric_limits<uint32_t>::max())
					valid_sample = samples[sample_idx];
			}

			// We may have an even distribution of samples and choose an invalid one in the previous loop
			// If there was a valid sample we use it, if not then all samples are invalid and is safe to use it
			if (best_sample == std::numeric_limits<uint32_t>::max())
				best_sample = valid_sample;

			indices_map[i * occlusion_map_width + j] = best_sample;
		}
	}

	delete[] index_map;
}
*/
