#include "FolderFiles.h"

#include "Formats/FileBytes.h"

#include <format>
#include <stdexcept>

void PrepareEmptyFolder(const std::filesystem::path& folder)
{
    if (std::filesystem::exists(folder))
    {
        if (!std::filesystem::is_directory(folder) || !std::filesystem::is_empty(folder))
            throw std::runtime_error(std::format("The folder {} is not empty", folder.string()));
    }

    std::filesystem::create_directories(folder);
}

void WriteBytesToFile(const std::filesystem::path& filePath, const Formats::Bytes& bytes)
{
    Formats::WriteFileBytes(filePath, bytes);
}
