#include "FolderFiles.h"

#include <format>
#include <fstream>
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
    std::ofstream file(filePath, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file)
        throw std::runtime_error(std::format("Cannot write the file {}", filePath.string()));
}
