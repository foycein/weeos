#include "Channels/WiiSpeak/WiiSpeakFiles.h"

#include "Channels/ChannelFiles.h"

#include "Formats/Lz77.h"
#include "Formats/Yaz0.h"

#include <string>

namespace Channels
{

namespace
{

constexpr std::uint16_t ResourceContentIndex = 2;
constexpr std::string_view SystemArchivePath = "./system.szs";
constexpr std::string_view SceneFolder = "./scene/";
constexpr std::string_view SceneExtension = ".szs";
constexpr std::string_view EnglishMessagesPath = "message/WiiMic_us_en.bmg";
constexpr std::string_view LayoutFontPath = "font/pac_nRodDb_32_I4.brfnt";
constexpr std::string_view LargeLayoutFontPath = "font/pac_nRodDb_200_IA4.brfnt";
constexpr std::string_view CheckMusicPath = "./eccheckbgm.bns";
constexpr std::uint16_t FacePartsContentIndex = 7;
constexpr std::string_view KeyboardScene = "keyboard";
constexpr std::string_view KeyboardBundle = "sofkeybd.arc";
constexpr std::string_view PhotoLayoutsPath = "./layout.arc.LZ";
constexpr std::string_view FacePartsPath = "RFL_Res.dat";

// The bundles keep blyt, anim and timg at their top, without the folder "arc".
const std::string BundleTopFolder = "";

Formats::U8Archive ReadPackedArchive(const Formats::U8Archive& archive, std::string_view path)
{
    return Formats::ReadU8Archive(Formats::DecompressYaz0(FindFileOrFail(archive, path)));
}

std::unique_ptr<Layout::LayoutArchive> ReadBundle(const Formats::U8Archive& archive, std::string_view bundleName)
{
    Formats::ByteView bundle = FindFileOrFail(archive, bundleName);
    return std::make_unique<Layout::LayoutArchive>(Formats::ReadU8Archive(bundle), BundleTopFolder);
}

}

WiiSpeakFiles::WiiSpeakFiles(SystemMenu::TitleContents& contents)
    : resources(ReadArchiveContent(contents, ResourceContentIndex))
    , systemArchive(ReadPackedArchive(resources, SystemArchivePath))
    , facePartsContent(ReadArchiveContent(contents, FacePartsContentIndex))
{
}

std::unique_ptr<Layout::LayoutArchive> WiiSpeakFiles::SystemLayouts(std::string_view bundleName) const
{
    return ReadBundle(systemArchive, bundleName);
}

std::unique_ptr<Layout::LayoutArchive> WiiSpeakFiles::SceneLayouts(std::string_view sceneName, std::string_view bundleName) const
{
    std::string scenePath = std::string(SceneFolder) + std::string(sceneName) + std::string(SceneExtension);
    Formats::U8Archive scene = ReadPackedArchive(resources, scenePath);
    return ReadBundle(scene, bundleName);
}

Formats::MessageFile WiiSpeakFiles::EnglishMessages() const
{
    return Formats::ReadMessageFile(FindFileOrFail(systemArchive, EnglishMessagesPath));
}

Formats::ByteView WiiSpeakFiles::LayoutFont() const
{
    return FindFileOrFail(systemArchive, LayoutFontPath);
}

Formats::ByteView WiiSpeakFiles::LargeLayoutFont() const
{
    return FindFileOrFail(systemArchive, LargeLayoutFontPath);
}

std::unique_ptr<Layout::LayoutArchive> WiiSpeakFiles::KeyboardLayouts() const
{
    std::string scenePath = std::string(SceneFolder) + std::string(KeyboardScene) + std::string(SceneExtension);
    Formats::U8Archive scene = ReadPackedArchive(resources, scenePath);
    return std::make_unique<Layout::LayoutArchive>(Formats::ReadU8Archive(FindFileOrFail(scene, KeyboardBundle)));
}

std::unique_ptr<Layout::LayoutArchive> WiiSpeakFiles::PhotoLayouts() const
{
    Formats::Bytes archive = Formats::DecompressLz77(FindFileOrFail(resources, PhotoLayoutsPath));
    return std::make_unique<Layout::LayoutArchive>(Formats::ReadU8Archive(archive));
}

Formats::ByteView WiiSpeakFiles::FaceParts() const
{
    return FindFileOrFail(facePartsContent, FacePartsPath);
}

Formats::ByteView WiiSpeakFiles::CheckMusic() const
{
    return FindFileOrFail(resources, CheckMusicPath);
}

}
