#include "Formats/FileBytes.h"

#include <format>
#include <fstream>
#include <stdexcept>

namespace Formats
{

Bytes ReadFileBytes(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error(std::format("Cannot open the file {}", path.string()));

    std::streamsize fileSize = file.tellg();
    file.seekg(0);

    Bytes bytes(static_cast<std::size_t>(fileSize));
    file.read(reinterpret_cast<char*>(bytes.data()), fileSize);
    if (!file)
        throw std::runtime_error(std::format("Cannot read the file {}", path.string()));

    return bytes;
}

void WriteFileBytes(const std::filesystem::path& path, ByteView bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file)
        throw std::runtime_error(std::format("Cannot write the file {}", path.string()));
}

}
