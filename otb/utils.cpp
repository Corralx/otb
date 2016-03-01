#include "utils.hpp"

#include "tinyobjloader/tiny_obj_loader.h"
#include "elektra/filesystem.hpp"

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
