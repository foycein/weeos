#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace Platform
{

// The file picker of Windows: the person chooses one file with this name, for example nand.bin.
// Gives back nothing when the person cancels. The program waits until the picker is closed.
std::optional<std::filesystem::path> PickFile(const std::wstring& title, const std::wstring& fileName);

}
