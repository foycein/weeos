#pragma once

#include "SystemMenu/NandFiles.h"

#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Title/TitleService.h"

namespace SystemMenu
{

// The files of the NAND, read, written and deleted through the file system service of IOS as the title that runs.
class IosNandFiles : public NandFiles
{
public:
    // The services must stay alive as long as this object.
    IosNandFiles(Ios::FileSystemService& fileSystemService, const Ios::TitleService& titleService);

    std::optional<Formats::Bytes> ReadFile(std::string_view path) override;
    void DeleteFile(std::string_view path) override;
    void WriteFile(std::string_view path, Formats::ByteView bytes) override;

private:
    Ios::NandPath ParsePath(std::string_view path) const;

    Ios::FileSystemService& fileSystem;
    const Ios::TitleService& titles;
};

}
