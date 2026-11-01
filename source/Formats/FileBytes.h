#pragma once

#include "Formats/Bytes.h"

#include <filesystem>

namespace Formats
{

// Reads a whole file. Throws std::runtime_error when the file cannot be read.
Bytes ReadFileBytes(const std::filesystem::path& path);

// Writes a whole file, and replaces the file that is there. Throws std::runtime_error when the file cannot be written.
void WriteFileBytes(const std::filesystem::path& path, ByteView bytes);

}
