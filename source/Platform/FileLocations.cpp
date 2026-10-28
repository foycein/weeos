#include "Platform/FileLocations.h"

#include "Platform/PlatformError.h"

#include <windows.h>

#include <format>
#include <string>

namespace Platform
{

std::filesystem::path ExecutableFolder()
{
    std::wstring path(MAX_PATH, L'\0');

    while (true)
    {
        DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0)
            throw PlatformError(std::format("The path of the program could not be read (Windows error {})", GetLastError()));

        // A path that does not fit is cut off and fills the whole buffer.
        if (length < path.size())
        {
            path.resize(length);
            return std::filesystem::path(path).parent_path();
        }

        path.resize(path.size() * 2);
    }
}

}
