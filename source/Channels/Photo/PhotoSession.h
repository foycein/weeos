#pragma once

#include "Channels/ChannelServices.h"
#include "Channels/Photo/PhotoLibrary.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/MenuLayout.h"

#include "Layout/LayoutArchive.h"

#include <memory>
#include <string_view>

namespace Channels
{

// The names of the sounds the screens ask for, as rev_photo.brsar names them. Which sound the original plays at which moment
// was not read from its program: these are Assumed (docs/audit/OpenPoints/Channels.md CHN-29).
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

    void GoTo(std::unique_ptr<PhotoScreen> screen);
    std::unique_ptr<PhotoScreen> TakeNextScreen();

    PhotoLibrary library;

private:
    ChannelServices& services;
    std::unique_ptr<Layout::LayoutArchive> layoutArchive;
    SystemMenu::MenuFonts fonts;
    std::unique_ptr<SystemMenu::CursorManager> cursors;
    std::unique_ptr<PhotoScreen> nextScreen;
};

// The screens. Each one is made when the channel goes to it.
std::unique_ptr<PhotoScreen> MakeTitleScreen(PhotoSession& session);
std::unique_ptr<PhotoScreen> MakeSdCardScreen(PhotoSession& session);
std::unique_ptr<PhotoScreen> MakeMessageBoardScreen(PhotoSession& session);
std::unique_ptr<PhotoScreen> MakeThumbnailScreen(PhotoSession& session, int firstShownFile);
std::unique_ptr<PhotoScreen> MakeViewScreen(PhotoSession& session, int fileIndex);
std::unique_ptr<PhotoScreen> MakeSlideShowScreen(PhotoSession& session, int firstFileIndex);

}
