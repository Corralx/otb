#include "utils.hpp"

#include "mesh.hpp"

#include "tinyobjloader/tiny_obj_loader.h"
#include "elektra/filesystem.hpp"
#include "elektra/file_io.hpp"
#include "GL/gl3w.h"
#include "glm/gtc/constants.hpp"

#pragma warning (push, 0)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#pragma warning (pop)

#include <random>

// TODO(Corralx): Signal errors in some way
// TODO(Corralx): Calculate smooth normals if not present
static void load_meshes_helper(const elk::path& path, std::vector<mesh_t>& meshes)
{
	if (path.empty() || !elk::exists(path) || path.extension() != ".obj")
		return;

	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
		
	std::string error;
	bool result = tinyobj::LoadObj(shapes, materials, error, path.c_str());
	if (!result)
		return;

	for (uint32_t i = 0; i < shapes.size(); ++i)
	{
		tinyobj::mesh_t& tiny_mesh = shapes[i].mesh;

		size_t num_vertices = tiny_mesh.positions.size() / 3;
		size_t num_normals = tiny_mesh.normals.size() / 3;
		size_t num_tex_coords = tiny_mesh.texcoords.size() / 2;
		size_t num_tris = tiny_mesh.indices.size() / 3;

		if (num_vertices == 0 || num_tris == 0 || num_normals != num_vertices || num_tex_coords != num_vertices)
			continue;

		std::vector<vertex_t> vertices(num_vertices);
		memcpy(vertices.data(), tiny_mesh.positions.data(), sizeof(vertex_t) * num_vertices);
		std::vector<normal_t> normals(num_normals);
		memcpy(normals.data(), tiny_mesh.normals.data(), sizeof(normal_t) * num_normals);
		std::vector<texture_coord_t> coords(num_tex_coords);
		memcpy(coords.data(), tiny_mesh.texcoords.data(), sizeof(texture_coord_t) * num_tex_coords);
		std::vector<face_t> faces(num_tris);
		memcpy(faces.data(), tiny_mesh.indices.data(), sizeof(face_t) * num_tris);
		
		mesh_t mesh(std::move(vertices), std::move(normals), std::move(coords), std::move(faces));
		meshes.push_back(std::move(mesh));
	}
}

std::vector<mesh_t> load_meshes(const elk::path& path)
{
	std::vector<mesh_t> meshes;
	load_meshes_helper(path, meshes);
	return std::move(meshes);
}

static void load_meshes_async_helper(const elk::path& path, std::vector<mesh_t>& meshes, std::promise<void> promise)
{
	load_meshes_helper(path, meshes);
	promise.set_value();
}

std::future<void> load_meshes_async(const elk::path& path, std::vector<mesh_t>& meshes)
{
	return async_apply(load_meshes_async_helper, std::ref(path), std::ref(meshes));
}

// NOTE(Corralx): An OpenGL context must be bound to the current thread for this to work
elk::optional<uint32_t> load_program(const elk::path& vs_path, const elk::path& fs_path, elk::optional<elk::path> gs_path)
{
	auto vs_source = elk::get_content_of_file(vs_path);
	auto fs_source = elk::get_content_of_file(fs_path);
	auto gs_source = gs_path ? elk::get_content_of_file(gs_path.value()) : elk::nullopt;

	if (!vs_source || !fs_source)
		return elk::nullopt;

	uint32_t program = glCreateProgram();

	const char* vs_ptr = vs_source.value().c_str();
	uint32_t vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vs_ptr, nullptr);
	glCompileShader(vs);
	glAttachShader(program, vs);

	const char* fs_ptr = fs_source.value().c_str();
	uint32_t fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fs_ptr, nullptr);
	glCompileShader(fs);
	glAttachShader(program, fs);

	uint32_t gs = 0;
	if (gs_source)
	{
		const char* gs_ptr = gs_source.value().c_str();
		gs = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(gs, 1, &gs_ptr, nullptr);
		glCompileShader(gs);
		glAttachShader(program, gs);
	}

	glLinkProgram(program);

	glDeleteShader(vs);
	if (gs != 0)
		glDeleteShader(gs);
	glDeleteShader(fs);

	assert(glGetError() == GL_NO_ERROR);
	return program;
}

template<>
bool write_image(const elk::path& path, const image<pixel_format::U8>& image, image_extension ext)
{
	assert(image.width() > 0);
	assert(image.height() > 0);
	assert(image.raw());

	switch (ext)
	{
		case image_extension::BMP:
			return stbi_write_bmp(path.c_str(), image.width(), image.height(), 1, image.raw()) != 0;

		case image_extension::PNG:
			return stbi_write_png(path.c_str(), image.width(), image.height(), 1, image.raw(), 0) != 0;

		case image_extension::TGA:
			return stbi_write_tga(path.c_str(), image.width(), image.height(), 1, image.raw()) != 0;

		default:
			assert(false);
	}

	return false;
}

template<>
bool write_image(const elk::path& path, const image<pixel_format::U32>& image, image_extension ext)
{
	assert(image.width() > 0);
	assert(image.height() > 0);
	assert(image.raw());

	switch (ext)
	{
		case image_extension::BMP:
			return stbi_write_bmp(path.c_str(), image.width(), image.height(), 4, image.raw()) != 0;

		case image_extension::PNG:
			return stbi_write_png(path.c_str(), image.width(), image.height(), 4, image.raw(), 0) != 0;

		case image_extension::TGA:
			return stbi_write_tga(path.c_str(), image.width(), image.height(), 4, image.raw()) != 0;

		default:
			assert(false);
	}

	return false;
}

bool write_image(const elk::path& path, const image<pixel_format::F32>& image)
{
	assert(image.width() > 0);
	assert(image.height() > 0);
	assert(image.raw());

	return stbi_write_hdr(path.c_str(), image.width(), image.height(), 1, image.raw()) != 0;
}

static bool same_side(const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& a, const glm::vec2& b)
{
	auto b_minus_a = glm::vec3(b.x - a.x, b.y - a.y, .0f);
	auto p1_minus_a = glm::vec3(p1.x - a.x, p1.y - a.y, .0f);
	auto p2_minus_a = glm::vec3(p2.x - a.x, p2.y - a.y, .0f);

	auto cp1 = glm::cross(b_minus_a, p1_minus_a);
	auto cp2 = glm::cross(b_minus_a, p2_minus_a);

	if (glm::dot(cp1, cp2) >= 0)
		return true;
	else
		return false;
}

bool point_in_tris(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c)
{
	return same_side(p, a, b, c) && same_side(p, b, a, c) && same_side(p, c, a, b);
}

static double random_double()
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_real_distribution<> dist(.0f, 1.f);

	return dist(gen);
}

// NOTE(Corralx): https://pathtracing.wordpress.com/2011/03/03/cosine-weighted-hemisphere/
glm::vec3 cosine_weighted_hemisphere_sample(glm::vec3 n)
{
	double xi1 = random_double();
	double xi2 = random_double();

	double  theta = acos(sqrt(1.0 - xi1));
	double  phi = 2.0 * glm::pi<double>() * xi2;

	float xs = static_cast<float>(sin(theta) * cos(phi));
	float ys = static_cast<float>(cos(theta));
	float zs = static_cast<float>(sin(theta) * sin(phi));

	glm::vec3 h = n;
	if (abs(h.x) <= abs(h.y) && abs(h.x) <= abs(h.z))
		h.x = 1.0;
	else if (abs(h.y) <= abs(h.x) && abs(h.y) <= abs(h.z))
		h.y = 1.0;
	else
		h.z = 1.0;

	glm::vec3 x = glm::normalize(glm::cross(h, n));
	glm::vec3 z = glm::normalize(glm::cross(x, n));

	glm::vec3 direction = xs * x + ys * n + zs * z;
	return glm::normalize(direction);
}

std::vector<float> generate_gaussian_kernel_1d(float sigma, uint32_t kernel_size)
{
	const int32_t kernel_half_size = static_cast<int32_t>(kernel_size) / 2;
	std::vector<float> kernel(kernel_size);
	float weight_sum = 0;

	auto gauss_generator = [](float x, float sigma)
	{
		float c = 2.f * sigma * sigma;
		return std::exp(-x * x / c) / std::sqrt(c * glm::pi<float>());
	};

	// kernel generation
	for (int32_t i = 0; i < static_cast<int32_t>(kernel_size); ++i)
	{
		float value = gauss_generator(static_cast<float>(i - kernel_half_size), sigma);
		kernel[i] = value;
		weight_sum += value;
	}

	// Normalization
	for (uint32_t i = 0; i < kernel_size; ++i)
		kernel[i] /= weight_sum;

	return std::move(kernel);
}

uint32_t generate_unique_index()
{
	static uint32_t next_free_index = 0;
	return next_free_index++;
}
