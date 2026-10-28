#pragma once

#include <filesystem>

namespace Platform
{

// The folder that holds the running program. The files that are built next to it, like the shaders, are found there.
std::filesystem::path ExecutableFolder();

}
