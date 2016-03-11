#include "render_manager.hpp"

#include "utils.hpp"
#include "configuration.hpp"

bool render_manager::init()
{
	if (!load_programs())
		return false;

	return true;
}

bool render_manager::load_programs()
{
	// TODO(Corralx)

	return true;
}
