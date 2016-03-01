#include "utils.hpp"

#include "tinyobjloader/tiny_obj_loader.h"
#include "elektra/filesystem.hpp"

#pragma warning (push, 0)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#pragma warning (pop)

#include <random>

static constexpr double PI = 3.14159265358979323846264338327950288;

std::vector<mesh_t> load_mesh(const elk::path& path)
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

		meshes.push_back(std::move(mesh));
	}

	return std::move(meshes);
}

template<>
bool write_image(const elk::path& path, const image<image_format::U8>& image, image_extension ext)
{
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

bool write_image(const elk::path& path, const image<image_format::F32>& image)
{
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
