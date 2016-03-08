#include "utils.hpp"

#include "tinyobjloader/tiny_obj_loader.h"
#include "elektra/filesystem.hpp"
#include "elektra/file_io.hpp"
#include "GL/gl3w.h"

#pragma warning (push, 0)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#pragma warning (pop)

#include <random>

// TODO(Corralx): having an additional shared context to load resource asynchronously
// NOTE(Corralx): Vertex Array Objects are not shared between context though!
std::vector<mesh_t> load_meshes(const elk::path& path)
{
	if (path.empty() || !elk::exists(path) || path.extension() != ".obj")
		return std::vector<mesh_t>();

	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
		
	std::string error;
	bool result = tinyobj::LoadObj(shapes, materials, error, path.c_str());
	if (!result)
		return std::vector<mesh_t>();

	std::vector<mesh_t> meshes;

	for (uint32_t i = 0; i < shapes.size(); ++i)
	{
		tinyobj::mesh_t& tiny_mesh = shapes[i].mesh;

		size_t num_vertices = tiny_mesh.positions.size() / 3;
		size_t num_normals = tiny_mesh.normals.size() / 3;
		size_t num_tex_coords = tiny_mesh.texcoords.size() / 2;
		size_t num_tris = tiny_mesh.indices.size() / 3;

		// TODO(Corralx): Signal errors in some way
		// TODO(Corralx): Calculate smooth normals if not present
		if (num_vertices == 0 || num_tris == 0 || num_normals != num_vertices || num_tex_coords != num_vertices)
			continue;

		mesh_t mesh;

		mesh._vertices.resize(num_vertices);
		memcpy(mesh._vertices.data(), tiny_mesh.positions.data(), sizeof(vertex_t) * num_vertices);
		mesh._normals.resize(num_normals);
		memcpy(mesh._normals.data(), tiny_mesh.normals.data(), sizeof(normal_t) * num_normals);
		mesh._coords.resize(num_tex_coords);
		memcpy(mesh._coords.data(), tiny_mesh.texcoords.data(), sizeof(texture_coord_t) * num_tex_coords);
		mesh._faces.resize(num_tris);
		memcpy(mesh._faces.data(), tiny_mesh.indices.data(), sizeof(face_t) * num_tris);

		mesh.init_buffers();

		meshes.push_back(std::move(mesh));
	}

	return std::move(meshes);
}

// TODO(Corralx): Load resource asynchronously
// NOTE(Corralx): An OpenGL context must be bound to the current thread for this to work
elk::optional<uint32_t> load_program(const elk::path& vs_path, const elk::path& fs_path)
{
	auto vs_source = elk::get_content_of_file(vs_path);
	auto fs_source = elk::get_content_of_file(fs_path);

	if (!vs_source || !fs_source)
		return elk::nullopt;

	const char* vs_ptr = vs_source.value().c_str();
	uint32_t vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vs_ptr, nullptr);
	glCompileShader(vs);

	const char* fs_ptr = fs_source.value().c_str();
	uint32_t fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fs_ptr, nullptr);
	glCompileShader(fs);

	uint32_t program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	assert(glGetError() == GL_NO_ERROR);

	glDeleteShader(vs);
	glDeleteShader(fs);

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
	double  phi = 2.0 * PI * xi2;

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
		return std::exp(-x * x / c) / std::sqrt(c * (float)PI);
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
