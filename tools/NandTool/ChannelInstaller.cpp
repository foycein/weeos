#include "ChannelInstaller.h"

#include "Formats/FileBytes.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/FileSystem/NandPreparation.h"
#include "Ios/FileSystem/NandStorage.h"
#include "Ios/Identity/ConsoleIdentityService.h"
#include "Ios/Title/TitleService.h"

#include <format>
#include <stdexcept>

std::vector<std::string> InstallChannelsTheDumpLacks(const std::filesystem::path& nandFolder, const std::filesystem::path& dumpFolder)
{
    Ios::Result<Ios::NandStorage> storage = Ios::NandStorage::Open(nandFolder);
    if (storage.Failed() || Ios::PrepareNandFolder(storage.Value()).Failed())
        throw std::runtime_error(std::format("The NAND folder {} cannot be opened to install channels", nandFolder.string()));

    Ios::Result<Ios::ConsoleIdentityService> identity = Ios::ConsoleIdentityService::Open(dumpFolder);
    if (identity.Failed())
        throw std::runtime_error(std::format("The identity of the dump in {} cannot be read", dumpFolder.string()));

    Ios::FileSystemService fileSystem(std::move(storage.Value()));
    Ios::TitleService titles(fileSystem, identity.Value());
    std::filesystem::path assetsFolder = dumpFolder.parent_path();

    std::vector<std::string> report;
    for (const ChannelTheDumpLacks& channel : ChannelsTheDumpLacks)
    {
        std::filesystem::path wadPath = assetsFolder / channel.wadInAssets;
        if (!std::filesystem::exists(wadPath))
        {
            report.push_back(std::format("{} is not installed: {} is missing (tools/fetch_titles.py fetches it)", channel.name, wadPath.string()));
            continue;
        }

        Ios::Result<void> installed = titles.InstallWad(Formats::ReadFileBytes(wadPath));
        if (installed.Failed())
        {
            report.push_back(std::format("{} is not installed: the title service refused it with {}", channel.name, int(installed.Error())));
            continue;
        }
        report.push_back(std::format("{} is installed from {}", channel.name, wadPath.string()));
    }
    return report;
}
