#include "Channels/Photo/PhotoSession.h"

#include "Channels/ChannelError.h"
#include "Channels/ChannelFiles.h"
#include "Channels/Photo/PhotoPictures.h"

#include "Formats/Font.h"

#include <array>
#include <string>

namespace Channels
{

namespace
{

// Content 2 of the Photo Channel 1.1 holds the layouts, the textures and the sounds; content 3 is the shared HOME Button Menu.
constexpr std::uint16_t ResourceContentIndex = 2;
constexpr std::uint16_t HomeMenuContentIndex = 3;
constexpr std::string_view LayoutArchivePath = "./layout.arc.LZ";
constexpr std::string_view FontPath = "arc/font/font_photo.brfnt";
constexpr std::string_view FontName = "font_photo.brfnt";
constexpr std::string_view TexturesPath = "./TPLPhoto.tpl.LZ";

// The hands are the ones of the channel's HOME Button Menu, HomeButton2: the channels' own hand is a NintendoWare effect that WeeOS
// cannot draw yet.
constexpr std::string_view HandArchivePath = "./HomeButton2/LZ77_homeBtn.arc";

// Every text and every picture with words is there once per language: the part of the pane, then T (text), I (picture with
// words) or M (the hints of the tips window), then the language. The program shows the English ones (US).
constexpr std::array<char, 3> LanguagePaneMarks = {'T', 'I', 'M'};
constexpr std::array<std::string_view, 6> OtherLanguages = {"JP", "GE", "FR", "IT", "SP", "DU"};
constexpr std::string_view ButtonPaneEnding = "B";

std::unique_ptr<Layout::LayoutArchive> ReadHands(SystemMenu::TitleContents& contents)
{
    Formats::U8Archive homeMenuFiles = ReadArchiveContent(contents, HomeMenuContentIndex);
    return std::make_unique<Layout::LayoutArchive>(Formats::ReadU8Archive(ReadPackedFile(homeMenuFiles, HandArchivePath)));
}

// The board's database as a channel reaches it: cdb.vff lies in the menu's data folder, which only the menu may change, but the file
// itself may be written by every title (rw rw rw on a real console). So the channel writes into the file as it is and never makes or deletes it.
class BoardFiles : public SystemMenu::NandFiles
{
public:
    BoardFiles(Ios::FileSystemService& fileSystemService, Ios::Caller channelCaller)
        : fileSystem(fileSystemService)
        , caller(channelCaller)
    {
    }

    std::optional<Formats::Bytes> ReadFile(std::string_view path) override
    {
        Ios::Result<Ios::OpenFile> file = fileSystem.Open(Ios::NandPath::Parse(path).Value(), Ios::OpenMode::Read, caller);
        if (file.Failed())
            return std::nullopt;

        Ios::Result<Ios::FileStatistics> statistics = file.Value().GetStatistics();
        if (statistics.Failed())
            return std::nullopt;

        Ios::Result<Formats::Bytes> bytes = file.Value().Read(statistics.Value().length);
        file.Value().Close();
        if (bytes.Failed())
            return std::nullopt;

        return bytes.Value();
    }

    void DeleteFile(std::string_view path) override
    {
        throw ChannelError("A channel cannot delete the file " + std::string(path) + " of the Message Board");
    }

    void WriteFile(std::string_view path, Formats::ByteView bytes) override
    {
        Ios::Result<Ios::OpenFile> file = fileSystem.Open(Ios::NandPath::Parse(path).Value(), Ios::OpenMode::Write, caller);
        if (file.Failed())
            throw ChannelError("The channel cannot write the file " + std::string(path) + " of the Message Board");

        Ios::Result<std::size_t> written = file.Value().Write(bytes);
        file.Value().Close();
        if (written.Failed() || written.Value() != bytes.size())
            throw ChannelError("The channel could not write all of " + std::string(path));
    }

private:
    Ios::FileSystemService& fileSystem;
    Ios::Caller caller;
};

bool IsPaneOfAnotherLanguage(std::string_view paneName)
{
    for (char mark : LanguagePaneMarks)
    {
        for (std::string_view language : OtherLanguages)
        {
            std::string languagePart = std::string(1, mark) + std::string(language);
            if (paneName.find(languagePart) != std::string_view::npos)
                return true;
        }
    }
    return false;
}

}

PhotoSession::PhotoSession(ChannelServices& channelServices)
    : storage(channelServices.storage)
    , services(channelServices)
{
    Formats::U8Archive resources = ReadArchiveContent(services.contents, ResourceContentIndex);
    channelTextures = Formats::ReadTplTextures(ReadPackedFile(resources, TexturesPath));
    Formats::U8Archive layoutFiles = Formats::ReadU8Archive(ReadPackedFile(resources, LayoutArchivePath));

    fonts.AddFont(services.display.Device(), Formats::ReadFont(FindFileOrFail(layoutFiles, FontPath)), {FontName});
    services.display.SetTextFonts(fonts.Fonts());

    layoutArchive = std::make_unique<Layout::LayoutArchive>(std::move(layoutFiles));
    cursors = std::make_unique<SystemMenu::CursorManager>(services.display, ReadHands(services.contents));
}

ChannelServices& PhotoSession::Services()
{
    return services;
}

SystemMenu::CursorManager& PhotoSession::Cursors()
{
    return *cursors;
}

std::unique_ptr<SystemMenu::MenuLayout> PhotoSession::MakeLayout(std::string_view layoutName)
{
    auto layout = std::make_unique<SystemMenu::MenuLayout>(services.display, *layoutArchive, std::string(layoutName));
    for (Formats::Pane* pane : layout->Live().AllPanes())
    {
        if (IsPaneOfAnotherLanguage(pane->name))
            pane->isVisible = false;
    }
    return layout;
}

bool PhotoSession::IsButtonPressed(SystemMenu::MenuLayout& layout, std::string_view buttonName)
{
    std::optional<Formats::Vector2> place = PlaceOfPress();
    if (!place)
        return false;

    Formats::Pane* button = layout.FindPane(buttonName);
    if (button == nullptr || !button->isVisible)
        return false;

    std::string areaName = std::string(buttonName) + std::string(ButtonPaneEnding);
    if (layout.FindPane(areaName) == nullptr)
        areaName = std::string(buttonName);

    return layout.IsPointInPane(areaName, *place);
}

bool PhotoSession::IsRemoteButtonPressed(Input::ButtonMask remoteButtons)
{
    return services.input.AnyPlayerPressed({remoteButtons, 0});
}

std::optional<Formats::Vector2> PhotoSession::PlaceOfPress()
{
    if (services.fader.GetStatus() != SystemMenu::FaderStatus::Clear)
        return std::nullopt;

    for (int playerIndex = 0; playerIndex < SystemMenu::PlayerCount; ++playerIndex)
    {
        const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(playerIndex);
        if (!player || !player->state.pointerIsValid)
            continue;

        if (services.input.HasPlayerPressed(playerIndex, SystemMenu::SelectButtons))
            return services.display.PointerToCanvas(player->state.pointer);
    }
    return std::nullopt;
}

PictureBox PhotoSession::BoxOfPane(SystemMenu::MenuLayout& layout, std::string_view paneName)
{
    const Formats::Pane& pane = layout.PaneNamed(paneName);
    Formats::Vector2 stretch = services.display.LayoutStretch();

    PictureBox box;
    box.centerX = pane.translation.x * stretch.x;
    box.centerY = pane.translation.y * stretch.y;
    box.width = pane.width * stretch.x;
    box.height = pane.height * stretch.y;
    return box;
}

std::optional<Formats::Bytes> PhotoSession::FileBytes(int fileIndex)
{
    const PhotoFile& file = library.files[fileIndex];
    if (file.boardPicture)
        return file.boardPicture;

    return ReadCardFile(services.storage.sdCard, file.path);
}

SystemMenu::MessageStore& PhotoSession::BoardStore()
{
    if (boardStore)
        return *boardStore;

    Ios::MacAddress address = services.network.configuration.GetWirelessMacAddress();
    SystemMenu::WirelessAddress wirelessAddress{};
    std::copy(address.begin(), address.end(), wirelessAddress.begin());
    boardFiles = std::make_unique<BoardFiles>(services.storage.fileSystem, services.storage.fileSystemCaller);
    boardStore = std::make_unique<SystemMenu::MessageStore>(*boardFiles, wirelessAddress);
    return *boardStore;
}

std::optional<Formats::RgbaImage> PhotoSession::PhotoOfFile(int fileIndex)
{
    auto changed = changedPhotos.find(fileIndex);
    if (changed != changedPhotos.end())
        return changed->second;

    return OriginalPhotoOfFile(fileIndex);
}

std::optional<Formats::RgbaImage> PhotoSession::OriginalPhotoOfFile(int fileIndex)
{
    const PhotoFile& file = library.files[fileIndex];
    std::optional<Formats::Bytes> bytes = FileBytes(fileIndex);
    if (!bytes)
        return std::nullopt;

    if (file.IsMovie())
        return FirstFrameOfMovie(*bytes, file.kind);

    return MakeShownPhoto(*bytes);
}

void PhotoSession::ChangePhoto(int fileIndex, const Formats::RgbaImage& changedPhoto)
{
    changedPhotos[fileIndex] = changedPhoto;
}

void PhotoSession::ForgetChanges(int fileIndex)
{
    changedPhotos.erase(fileIndex);
}

bool PhotoSession::HasChangedPhotos() const
{
    return !changedPhotos.empty();
}

bool PhotoSession::IsPhotoChanged(int fileIndex) const
{
    return changedPhotos.contains(fileIndex);
}

void PhotoSession::PlayDecodedSound(const Media::PcmSound& sound)
{
    constexpr float SixteenBitFullScale = 32768.0f;
    StopDecodedSound();
    if (sound.channelCount <= 0)
        return;

    auto wave = std::make_shared<Sound::DecodedWave>();
    wave->sampleRate = static_cast<std::uint32_t>(sound.sampleRate);
    wave->loops = false;
    wave->loopStartSample = 0;
    wave->channels.assign(sound.channelCount, std::vector<float>(sound.FrameCount()));
    for (std::size_t frame = 0; frame < sound.FrameCount(); ++frame)
    {
        for (int channel = 0; channel < sound.channelCount; ++channel)
            wave->channels[channel][frame] = sound.samples[frame * sound.channelCount + channel] / SixteenBitFullScale;
    }
    decodedSound = services.audio.waves.Play(wave);
}

void PhotoSession::StopDecodedSound()
{
    services.audio.waves.Stop(decodedSound);
    decodedSound = {};
}

bool PhotoSession::IsDecodedSoundPlaying()
{
    return services.audio.waves.IsPlaying(decodedSound);
}

const Layout::DrawableFont& PhotoSession::Font() const
{
    return fonts.Fonts().Find(std::string(FontName));
}

const Formats::RgbaImage& PhotoSession::ChannelTexture(int textureIndex)
{
    auto decoded = decodedTextures.find(textureIndex);
    if (decoded != decodedTextures.end())
        return decoded->second;

    decodedTextures[textureIndex] = Formats::DecodeTexture(channelTextures.at(textureIndex));
    return decodedTextures[textureIndex];
}

void PhotoSession::GoTo(std::unique_ptr<PhotoScreen> screen)
{
    nextScreen = std::move(screen);
}

std::unique_ptr<PhotoScreen> PhotoSession::TakeNextScreen()
{
    return std::move(nextScreen);
}

}
