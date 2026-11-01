#pragma once

#include "Formats/Bytes.h"

#include <filesystem>

// Throws a runtime_error when the folder exists and is not empty (or is a file). Otherwise makes the folder, and the folders above it.
void PrepareEmptyFolder(const std::filesystem::path& folder);

// Writes a whole file. Throws a runtime_error when it cannot be written.
void WriteBytesToFile(const std::filesystem::path& filePath, const Formats::Bytes& bytes);
