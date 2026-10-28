#pragma once

#include "Formats/Bytes.h"

#include <filesystem>

namespace Formats
{

// Reads a whole file. Throws std::runtime_error when the file cannot be read.
Bytes ReadFileBytes(const std::filesystem::path& path);

}
