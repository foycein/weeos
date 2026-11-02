#include "Ios/Identity/SharedKeys.h"

#include "Formats/FileBytes.h"
#include "Formats/HexText.h"
#include "Formats/NandTitles.h"
#include "Formats/Sha256.h"

#include <algorithm>
#include <format>
#include <map>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace Ios
{

namespace
{

constexpr std::string_view TitleMetadataFileName = "title.tmd";

// The programs keep their keys on multiples of four bytes.
constexpr std::size_t KeyAlignment = 4;

// Only the SHA-256 digests of the keys are written here, never the keys themselves.
constexpr std::string_view CommonKeyDigest = "de38aeab4fe0c36d828a47e6fd315100e7ce234d3b00aa25e6ad6f5ff2824af8";
constexpr std::string_view SdKeyDigest = "02c596910cebf5af0a03fe6ab0b36c5ba2a4f2ff49abbf387998c5d31e350666";
constexpr std::string_view SdInitializationVectorDigest = "b652fa375ce93ee3fad92f7d8539fff29b7226f197e177adb4916c3e1e3d5da3";
constexpr std::string_view Md5BlankerDigest = "ff09801a73966b97b99b66e930ba7175f6a746c6f8151c184845663414079cd6";

using FoundKeys = std::map<Formats::Sha256Digest, Formats::AesBlock>;

class WiiFilesContents : public TitleContents
{
public:
    explicit WiiFilesContents(const std::filesystem::path& folder)
        : wiiFilesFolder(folder)
    {
    }

    Formats::TitleMetadata ReadMetadata(TitleId titleId) override
    {
        std::filesystem::path metadataPath = TitleFolder(titleId) / TitleMetadataFileName;
        if (!std::filesystem::exists(metadataPath))
            throw std::runtime_error(std::format("The title {} is not in the Wii files", titleId.ToString()));

        return Formats::ReadTitleMetadata(Formats::ReadFileBytes(metadataPath));
    }

    Formats::Bytes ReadContent(TitleId titleId, const Formats::ContentRecord& record) override
    {
        return Formats::ReadFileBytes(TitleFolder(titleId) / std::format("{:08x}.app", record.id));
    }

private:
    std::filesystem::path TitleFolder(TitleId titleId) const
    {
        return wiiFilesFolder / std::format("{:016x}", titleId.ToValue());
    }

    std::filesystem::path wiiFilesFolder;
};

class NandContents : public TitleContents
{
public:
    explicit NandContents(Formats::NandFileSystem& fileSystem)
        : titles(fileSystem)
    {
    }

    Formats::TitleMetadata ReadMetadata(TitleId titleId) override
    {
        return titles.ReadMetadata(titleId.ToValue());
    }

    Formats::Bytes ReadContent(TitleId titleId, const Formats::ContentRecord& record) override
    {
        return titles.ReadContent(titleId.ToValue(), record);
    }

private:
    Formats::NandTitles titles;
};

Formats::Sha256Digest DigestFromHex(std::string_view hexText)
{
    Formats::Bytes bytes = Formats::ParseHexText(hexText);
    Formats::Sha256Digest digest;
    std::copy(bytes.begin(), bytes.end(), digest.begin());
    return digest;
}

// Looks at every 16 bytes of the program that start on a multiple of four and keeps the ones whose digest is one of the wanted digests.
void SearchProgram(Formats::ByteView program, const std::vector<Formats::Sha256Digest>& wantedDigests, FoundKeys& foundKeys)
{
    for (std::size_t offset = 0; offset + Formats::AesBlockSize <= program.size(); offset += KeyAlignment)
    {
        Formats::ByteView candidate = program.subspan(offset, Formats::AesBlockSize);
        Formats::Sha256Digest digest = Formats::ComputeSha256(candidate);
        bool isWanted = std::find(wantedDigests.begin(), wantedDigests.end(), digest) != wantedDigests.end();
        if (!isWanted)
            continue;

        Formats::AesBlock key;
        std::copy(candidate.begin(), candidate.end(), key.begin());
        foundKeys[digest] = key;
    }
}

Formats::AesBlock TakeKey(const FoundKeys& foundKeys, std::string_view digestHex, std::string_view keyName)
{
    FoundKeys::const_iterator place = foundKeys.find(DigestFromHex(digestHex));
    if (place == foundKeys.end())
        throw std::runtime_error(std::format("The System Menu and its IOS do not hold {}", keyName));

    return place->second;
}

}

SharedKeys FindSharedKeys(TitleContents& titles)
{
    std::vector<Formats::Sha256Digest> wantedDigests = {
        DigestFromHex(CommonKeyDigest),
        DigestFromHex(SdKeyDigest),
        DigestFromHex(SdInitializationVectorDigest),
        DigestFromHex(Md5BlankerDigest),
    };
    FoundKeys foundKeys;

    Formats::TitleMetadata menuMetadata = titles.ReadMetadata(SystemMenuTitleId);
    for (const Formats::ContentRecord& record : menuMetadata.contents)
    {
        if (record.index == menuMetadata.bootIndex)
            SearchProgram(titles.ReadContent(SystemMenuTitleId, record), wantedDigests, foundKeys);
    }

    TitleId iosTitleId = TitleId::FromValue(menuMetadata.requiredSystemVersion);
    for (const Formats::ContentRecord& record : titles.ReadMetadata(iosTitleId).contents)
        SearchProgram(titles.ReadContent(iosTitleId, record), wantedDigests, foundKeys);

    SharedKeys keys;
    keys.commonKey = TakeKey(foundKeys, CommonKeyDigest, "the common key");
    keys.sdKey = TakeKey(foundKeys, SdKeyDigest, "the SD key");
    keys.sdInitializationVector = TakeKey(foundKeys, SdInitializationVectorDigest, "the initialization vector of the SD card");
    keys.md5Blanker = TakeKey(foundKeys, Md5BlankerDigest, "the digest placeholder of the SD card");
    return keys;
}

SharedKeys FindSharedKeysInWiiFiles(const std::filesystem::path& wiiFilesFolder)
{
    WiiFilesContents titles(wiiFilesFolder);
    return FindSharedKeys(titles);
}

SharedKeys FindSharedKeysInNand(Formats::NandFileSystem& nandFileSystem)
{
    NandContents titles(nandFileSystem);
    return FindSharedKeys(titles);
}

}
