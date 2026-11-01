#pragma once

#include "Ios/FileSystem/NandPath.h"
#include "Ios/Title/TitleId.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Ios
{

// Where the title service keeps its files on the NAND. The names are the ones of the console:
//
//   /title/<high>/<low>/content/title.tmd     the title metadata of an installed title
//   /title/<high>/<low>/content/<id>.app      a content that only this title has
//   /title/<high>/<low>/data                  the folder the title keeps its own files in
//   /ticket/<high>/<low>.tik                  the tickets of a title, one after the other
//   /shared1/<name>.app                       a content that several titles share, named in /shared1/content.map by its hash
//   /sys/uid.sys                              the user id of every title
//   /import/<high>/<low>/content/...          a title that is being installed

NandPath TitlesPath();
NandPath TitleFamilyPath(std::uint32_t highPart);
NandPath TitlePath(TitleId titleId);
NandPath TitleMetadataPath(TitleId titleId);
NandPath ContentFolderPath(TitleId titleId);
NandPath PrivateContentPath(TitleId titleId, std::uint32_t contentId);
NandPath DataFolderPath(TitleId titleId);

NandPath TicketsPath();
NandPath TicketFamilyPath(std::uint32_t highPart);
NandPath TicketFilePath(TitleId titleId);

NandPath SharedContentFolderPath();
NandPath SharedContentMapPath();
NandPath SharedContentPath(std::string_view name);

NandPath UserIdFilePath();
NandPath CertificateFilePath();

NandPath ImportsPath();
NandPath ImportFamilyPath(std::uint32_t highPart);
NandPath ImportTitlePath(TitleId titleId);
NandPath ImportContentFolderPath(TitleId titleId);
NandPath ImportMetadataPath(TitleId titleId);
NandPath ImportContentPath(TitleId titleId, std::uint32_t contentId);

// Where the title metadata and a content wait while they are written and checked, before they are moved to their place.
NandPath TemporaryMetadataPath();
NandPath TemporaryContentPath(std::uint32_t contentId);
NandPath TemporarySharedContentPath(std::string_view name);

// A folder or file name that is a number of eight hexadecimal digits: "00010002", "48414341".
std::string FormatNameNumber(std::uint32_t number);

// The number in such a name, or nothing when the name is not eight hexadecimal digits.
std::optional<std::uint32_t> ParseNameNumber(std::string_view name);

}
