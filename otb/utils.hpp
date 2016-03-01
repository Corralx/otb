#pragma once

#include "mesh.hpp"

namespace elk
{
class path;
}

std::vector<mesh_t> load_mesh(const elk::path& path);
