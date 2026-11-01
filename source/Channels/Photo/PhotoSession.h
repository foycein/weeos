#pragma once

#include "Channels/ChannelServices.h"
#include "Channels/Photo/PhotoLibrary.h"
#include "Channels/Photo/PhotoStorage.h"
#include "Channels/Photo/ShownPicture.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/IosNandFiles.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MessageBoard/MessageStore.h"

#include "Layout/LayoutArchive.h"

#include "Formats/TplTexture.h"

#include <map>
#include <memory>
#include <optional>
#include <string_view>

namespace Channels
{

// The names of the sounds the screens ask for, as rev_photo.brsar names them. Which sound the original plays at which moment
// was not read from its program: these are Assumed (docs/audit/OpenPoints/Channels.md CHN-31).
namespace PhotoSound
{

constexpr std::string_view MainMusic = "BGM_MAIN";
constexpr std::string_view SlideShowMusic = "BGM_SLIDE1";
constexpr std::string_view Decide = "SE_KETTEI_A";
constexpr std::string_view Cancel = "SE_CANCEL";
constexpr std::string_view Scroll = "SE_SCROLL";
constexpr std::string_view NextPhoto = "SE_PHOTO_NEXT";
constexpr std::string_view SelectPhoto = "SE_PHOTO_SELECT";
constexpr std::string_view Turn = "SE_KAITEN";
constexpr std::string_view ZoomIn = "SE_KAKUDAI1";
constexpr std::string_view ZoomOut = "SE_SHUKUSHO1";
constexpr std::string_view Warning = "SE_WARNING";

}

// One screen of the Photo Channel: a few layouts of layout.arc and what the channel draws over them.
class PhotoScreen
{
public:
    PhotoScreen() = default;
    PhotoScreen(const PhotoScreen&) = delete;
    PhotoScreen& operator=(const PhotoScreen&) = delete;
    virtual ~PhotoScreen() = default;

    virtual void Update() = 0;
    virtual void Draw() = 0;

    // What the screen shows, in words, for the messages of the tests: for example "Thumbnails" or "No SD card".
    virtual std::string_view Name() const = 0;
};

// What every screen of the channel shares: the engines, the files of the title, the hands, what the card holds, and the way to
// the next screen (the channel fades out, changes the screen and fades in).
class PhotoSession
{
public:
    explicit PhotoSession(ChannelServices& channelServices);

    ChannelServices& Services();
    SystemMenu::CursorManager& Cursors();

    // A layout of layout.arc with the panes of the other languages hidden.
    std::unique_ptr<SystemMenu::MenuLayout> MakeLayout(std::string_view layoutName);

    // True in the step in which a player pressed A while pointing at the button: the pane "<buttonName>B" of the layout, or the
    // pane itself when the button is a bounding pane of its own.
    bool IsButtonPressed(SystemMenu::MenuLayout& layout, std::string_view buttonName);

    // True in the step in which a player pressed one of the buttons of the remote.
    bool IsRemoteButtonPressed(Input::ButtonMask remoteButtons);

    // The place a player points at, in canvas units, when a player pressed A in this step.
    std::optional<Formats::Vector2> PlaceOfPress();

    // The bytes of a file of the library: the picture of a board letter, or the file of the card.
    std::optional<Formats::Bytes> FileBytes(int fileIndex);

    // The Message Board's database (cdb.vff in the System Menu's data folder), opened the first time it is needed.
    SystemMenu::MessageStore& BoardStore();

    // Where a pane of a layout lies on the screen, in canvas units (the panes named "picture", where the channel draws the photo).
    PictureBox BoxOfPane(SystemMenu::MenuLayout& layout, std::string_view paneName);

    // The photo of a file as Fun! has left it, else as the card holds it (the first frame of a movie). Nothing for a file that
    // cannot be shown.
    std::optional<Formats::RgbaImage> PhotoOfFile(int fileIndex);
    std::optional<Formats::RgbaImage> OriginalPhotoOfFile(int fileIndex);
    void ChangePhoto(int fileIndex, const Formats::RgbaImage& changedPhoto);
    void ForgetChanges(int fileIndex);
    bool HasChangedPhotos() const;
    bool IsPhotoChanged(int fileIndex) const;

    // The font of the channel, font_photo.brfnt.
    const Layout::DrawableFont& Font() const;

    // A picture of TPLPhoto.tpl (the stamps of Doodle, the background, the channel's own hands).
    const Formats::RgbaImage& ChannelTexture(int textureIndex);

    void GoTo(std::unique_ptr<PhotoScreen> screen);
    std::unique_ptr<PhotoScreen> TakeNextScreen();

    PhotoLibrary library;
    PhotoStorage storage;

private:
    ChannelServices& services;
    std::unique_ptr<Layout::LayoutArchive> layoutArchive;
    SystemMenu::MenuFonts fonts;
    std::unique_ptr<SystemMenu::CursorManager> cursors;
    std::unique_ptr<PhotoScreen> nextScreen;
    std::map<int, Formats::RgbaImage> changedPhotos;
    std::vector<Formats::TplTexture> channelTextures;
    std::map<int, Formats::RgbaImage> decodedTextures;
    std::unique_ptr<SystemMenu::IosNandFiles> boardFiles;
    std::unique_ptr<SystemMenu::MessageStore> boardStore;
};

// The screens. Each one is made when the channel goes to it.
std::unique_ptr<PhotoScreen> MakeTitleScreen(PhotoSession& session);
std::unique_ptr<PhotoScreen> MakeSdCardScreen(PhotoSession& session);
std::unique_ptr<PhotoScreen> MakeMessageBoardScreen(PhotoSession& session);
std::unique_ptr<PhotoScreen> MakeThumbnailScreen(PhotoSession& session, int firstShownFile);
std::unique_ptr<PhotoScreen> MakeViewScreen(PhotoSession& session, int fileIndex);
std::unique_ptr<PhotoScreen> MakeSlideShowScreen(PhotoSession& session, int firstFileIndex);
std::unique_ptr<PhotoScreen> MakeSlideSettingsScreen(PhotoSession& session, int fileIndex);
std::unique_ptr<PhotoScreen> MakeFunScreen(PhotoSession& session, int fileIndex);
std::unique_ptr<PhotoScreen> MakeMoodScreen(PhotoSession& session, int fileIndex);
std::unique_ptr<PhotoScreen> MakeDoodleScreen(PhotoSession& session, int fileIndex);
std::unique_ptr<PhotoScreen> MakePuzzleScreen(PhotoSession& session, int fileIndex);
std::unique_ptr<PhotoScreen> MakePostScreen(PhotoSession& session, int fileIndex);
std::unique_ptr<PhotoScreen> MakeSaveErrorScreen(PhotoSession& session, PhotoStorage::StartResult result);

}
