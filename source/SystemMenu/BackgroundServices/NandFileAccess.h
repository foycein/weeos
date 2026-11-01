#pragma once

#include "Formats/Bytes.h"
#include "Ios/FileSystem/EntryPermissions.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/FileSystem/NandPath.h"
#include "Ios/Result.h"
#include "Ios/Title/TitleService.h"

#include <string_view>

namespace SystemMenu
{

// The rights of the files that the menu makes, as the dump shows them.
constexpr Ios::EntryPermissions EverybodyReadsAndWrites = {Ios::AccessRights::ReadWrite, Ios::AccessRights::ReadWrite, Ios::AccessRights::ReadWrite};
constexpr Ios::EntryPermissions OthersOnlyRead = {Ios::AccessRights::ReadWrite, Ios::AccessRights::Read, Ios::AccessRights::Read};
constexpr Ios::EntryPermissions OthersHaveNoRights = {Ios::AccessRights::ReadWrite, Ios::AccessRights::ReadWrite, Ios::AccessRights::None};

// A path that the program spells out, like "/shared2/sys/flags.dat". A path the NAND does not accept is a mistake of the program and throws.
Ios::NandPath MenuPath(std::string_view text);

// The files of the NAND as the menu reads and writes them: through the file system of IOS, as the title that runs
// (the System Menu, or the title the menu has become for a moment).
class NandFileAccess
{
public:
    // The services must stay alive as long as this object.
    NandFileAccess(Ios::FileSystemService& fileSystem, const Ios::TitleService& titles);

    // The whole file. NotFound when there is no such file.
    Ios::Result<Formats::Bytes> Read(const Ios::NandPath& path);

    Ios::Result<std::uint32_t> Length(const Ios::NandPath& path);

    // Owner, group, permissions and flags of a file or directory.
    Ios::Result<Ios::EntryAttributes> Attributes(const Ios::NandPath& path);

    // Makes an empty file. AlreadyExists when the file is there.
    Ios::Result<void> Create(const Ios::NandPath& path, const Ios::EntryPermissions& permissions);

    // Writes the bytes at the start of the file. A longer file keeps the rest of its bytes, as on the console.
    Ios::Result<void> Write(const Ios::NandPath& path, Formats::ByteView bytes);

    // Writes the bytes behind the end of the file.
    Ios::Result<void> Append(const Ios::NandPath& path, Formats::ByteView bytes);

    Ios::Result<void> Delete(const Ios::NandPath& path);

    // The names in a directory.
    Ios::Result<std::vector<std::string>> List(const Ios::NandPath& directory);

    Ios::FileSystemService& FileSystem();

private:
    Ios::Result<void> WriteAt(const Ios::NandPath& path, Formats::ByteView bytes, Ios::SeekOrigin origin);

    Ios::FileSystemService& fileSystem;
    const Ios::TitleService& titles;
};

}
