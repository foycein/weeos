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

}
