#pragma once

#include "Formats/Aes128.h"
#include "Formats/Bytes.h"
#include "Formats/Md5.h"
#include "Formats/NandFileSystem.h"
#include "Formats/TitleMetadata.h"
#include "Ios/Title/TitleId.h"

#include <filesystem>

namespace Ios
{

// The keys that every Wii has. WeeOS keeps none of them in its code: they come from the person's own console, whose programs hold them.
// IOS keeps the common key and the SD key in its module ES; the System Menu keeps the initialization vector of the SD card and the
// bytes that stand in for the digest of a header on the SD card (md5_blanker of keys.txt).
struct SharedKeys
{
    Formats::AesKey commonKey;
    Formats::AesKey sdKey;
    Formats::AesBlock sdInitializationVector;
    Formats::Md5Digest md5Blanker;
};

// The installed titles of a console, with their contents decrypted: the prepared Wii files, or the NAND of a backup.
class TitleContents
{
public:
    virtual ~TitleContents() = default;

    virtual Formats::TitleMetadata ReadMetadata(TitleId titleId) = 0;
    virtual Formats::Bytes ReadContent(TitleId titleId, const Formats::ContentRecord& record) = 0;
};

// Finds the shared keys in the program of the System Menu and in the contents of the IOS it runs on: they are the 16 bytes whose
// SHA-256 digest is the digest of the key. Throws std::runtime_error when a key was not found, and what the titles throw when the
// System Menu or its IOS cannot be read.
SharedKeys FindSharedKeys(TitleContents& titles);

// The same, in a folder of prepared Wii files: a folder for each title, named after its id ("0000000100000002"),
// with title.tmd and the contents, each named after its content id ("00000098.app").
SharedKeys FindSharedKeysInWiiFiles(const std::filesystem::path& wiiFilesFolder);

// The same, in the NAND of a backup.
SharedKeys FindSharedKeysInNand(Formats::NandFileSystem& nandFileSystem);

}
