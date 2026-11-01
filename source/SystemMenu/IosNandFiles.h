#pragma once

#include "SystemMenu/NandFiles.h"

#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Title/TitleService.h"

#include <atomic>
#include <cstdint>
namespace SystemMenu
{

// The files of the NAND, read, written and deleted through the file system service of IOS as the title that runs.
class IosNandFiles : public NandFiles
{
public:
    // The services must stay alive as long as this object.
    IosNandFiles(Ios::FileSystemService& fileSystemService, const Ios::TitleService& titleService);

    // As a caller that stays the same, for a channel that knows who it is.
    IosNandFiles(Ios::FileSystemService& fileSystemService, Ios::Caller fixedCaller);

    std::optional<Formats::Bytes> ReadFile(std::string_view path) override;
    void DeleteFile(std::string_view path) override;
    void WriteFile(std::string_view path, Formats::ByteView bytes) override;

private:
    Ios::NandPath ParsePath(std::string_view path) const;
    Ios::Caller CurrentCaller() const;
    Ios::NandPath MakeSafeSaveFolder(std::string_view pathForErrors);
    void WriteNewFile(const Ios::NandPath& nandPath, std::string_view pathForErrors, Formats::ByteView bytes);

    Ios::FileSystemService& fileSystem;
    const Ios::TitleService* titles = nullptr;
    Ios::Caller caller = Ios::SuperUserCaller;

    // Every safe save has a folder of its own, numbered as the NAND library numbers them.
    std::atomic<std::uint32_t> nextSafeSaveNumber = 0;
};

}
