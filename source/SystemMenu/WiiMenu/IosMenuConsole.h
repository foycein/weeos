#pragma once

#include "SystemMenu/WiiMenu/MenuConsole.h"

#include "Ios/IosServices.h"

namespace SystemMenu
{

// The console of the menu on the services of IOS: the titles come from the title service, the files from the file system,
// and the time from the clock of the PC. The title service must have entered the System Menu, because the menu reads and writes
// its files as that title. The services must stay alive as long as this object.
class IosMenuConsole : public MenuConsole
{
public:
    explicit IosMenuConsole(Ios::IosServices& iosServices);

    std::vector<InstalledTitle> ListInstalledTitles() override;
    Formats::Bytes ReadBannerContent(Ios::TitleId titleId) override;
    std::optional<Formats::Bytes> ReadNandFile(std::string_view path) override;

    // The files of the menu have a fixed size: the new bytes must be as long as the file that is there. A file that is not there is made.
    void WriteNandFile(std::string_view path, Formats::ByteView bytes) override;
    LocalTime GetLocalTime() override;

private:
    InstalledTitle DescribeTitle(Ios::TitleId titleId);
    Ios::Result<Ios::OpenFile> MakeFileAndOpenIt(const Ios::NandPath& path, const Ios::Caller& caller);

    Ios::TitleService& titles;
    Ios::FileSystemService& fileSystem;
};

}
