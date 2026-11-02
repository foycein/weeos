#pragma once

#include "Formats/Aes128.h"
#include "Formats/Bytes.h"
#include "Ios/Result.h"
#include "Ios/Title/TitleService.h"

#include <filesystem>

// The Wii files that ship beside WeeOS.exe: every title in a folder of its own, named after its id (0000000100000002), with
//   title.tmd          the title metadata, signed by Nintendo
//   ticket.tik         the ticket, signed by Nintendo
//   certificates.bin   the certificates that vouch for both
//   <content id>.app   every content, decrypted (00000097.app)
// A new console installs them through the title service, which checks the signatures and the hash of every content.

// Writes the title of a WAD file (as Nintendo's update server gives it) into its folder below the folder of the Wii files, with every
// content decrypted with the common key and checked against its hash. Gives back the folder of the title.
// Throws a FormatError when the WAD file cannot be read or a content does not fit its hash, std::runtime_error when a file cannot be written.
std::filesystem::path PrepareTitleFromWad(Formats::ByteView wadFile, const Formats::AesKey& commonKey, const std::filesystem::path& wiiFilesFolder);

// Installs one prepared title through the title service: the ticket, the title metadata, the contents and the end.
// The answer is the first error of the title service. A file that cannot be read throws std::runtime_error.
Ios::Result<void> InstallPreparedTitle(Ios::TitleService& titles, const std::filesystem::path& titleFolder);
