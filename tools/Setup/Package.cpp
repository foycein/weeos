#include "Package.h"

#include <windows.h>

#include <stdexcept>

namespace Setup
{

namespace
{

constexpr wchar_t PackageResourceName[] = L"PACKAGE";
constexpr wchar_t ProgramNamesResourceName[] = L"PROGRAMNAMES";
const WORD ResourceLanguage = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

constexpr wchar_t NameSeparator = L'\n';

Formats::ByteView FindCarriedResource(const wchar_t* resourceName)
{
    HRSRC resource = FindResourceW(nullptr, resourceName, RT_RCDATA);
    if (resource == nullptr)
        return {};

    HGLOBAL loadedResource = LoadResource(nullptr, resource);
    const auto* bytes = static_cast<const Formats::Byte*>(LockResource(loadedResource));
    return Formats::ByteView(bytes, SizeofResource(nullptr, resource));
}

std::wstring JoinNames(const std::vector<std::wstring>& names)
{
    std::wstring text;
    for (const std::wstring& name : names)
    {
        text += name;
        text += NameSeparator;
    }
    return text;
}

std::vector<std::wstring> SplitNames(std::wstring_view text)
{
    std::vector<std::wstring> names;
    std::wstring name;
    for (wchar_t character : text)
    {
        if (character != NameSeparator)
        {
            name += character;
            continue;
        }

        names.push_back(name);
        name.clear();
    }
    return names;
}

void ChangeResources(const std::filesystem::path& programFile, const std::wstring& programNamesText)
{
    BOOL removeAllOtherResources = FALSE;
    HANDLE update = BeginUpdateResourceW(programFile.c_str(), removeAllOtherResources);
    if (update == nullptr)
        throw std::runtime_error("The uninstaller could not be written.");

    void* noBytes = nullptr;
    bool packageRemoved = UpdateResourceW(update, RT_RCDATA, PackageResourceName, ResourceLanguage, noBytes, 0);

    auto* textBytes = const_cast<wchar_t*>(programNamesText.data());
    DWORD textSize = static_cast<DWORD>(programNamesText.size() * sizeof(wchar_t));
    bool namesAdded = UpdateResourceW(update, RT_RCDATA, ProgramNamesResourceName, ResourceLanguage, textBytes, textSize);

    BOOL throwChangesAway = !(packageRemoved && namesAdded);
    bool written = EndUpdateResourceW(update, throwChangesAway);
    if (!written || throwChangesAway)
        throw std::runtime_error("The uninstaller could not be written.");
}

}

bool ThisProgramCarriesThePackage()
{
    return !FindCarriedResource(PackageResourceName).empty();
}

Formats::ByteView CarriedPackage()
{
    return FindCarriedResource(PackageResourceName);
}

std::vector<std::wstring> CarriedProgramNames()
{
    Formats::ByteView bytes = FindCarriedResource(ProgramNamesResourceName);
    const auto* characters = reinterpret_cast<const wchar_t*>(bytes.data());
    return SplitNames(std::wstring_view(characters, bytes.size() / sizeof(wchar_t)));
}

std::filesystem::path ThisProgramFile()
{
    std::wstring path(MAX_PATH, L'\0');
    while (true)
    {
        DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length < path.size())
        {
            path.resize(length);
            return path;
        }

        path.resize(path.size() * 2);
    }
}

void WriteUninstaller(const std::filesystem::path& uninstallerFile, const std::vector<std::wstring>& programNames)
{
    if (!CopyFileW(ThisProgramFile().c_str(), uninstallerFile.c_str(), FALSE))
        throw std::runtime_error("The uninstaller could not be written. Is WeeOS still running?");

    ChangeResources(uninstallerFile, JoinNames(programNames));
}

}
