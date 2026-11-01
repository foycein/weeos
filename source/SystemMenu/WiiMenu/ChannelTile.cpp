#include "SystemMenu/WiiMenu/ChannelTile.h"

#include "SystemMenu/WiiMenu/LayoutParts.h"

#include <algorithm>
#include <array>

namespace SystemMenu
{

namespace
{

constexpr std::string_view IconLayout = "icon";
constexpr std::string_view IconAnimation = "icon";
constexpr std::string_view EmptySlotLayout = "my_IplTop_b";
constexpr std::string_view EmptySlotAnimation = "my_IplTop_b";
constexpr std::string_view DiscLayout = "my_DiskCh_b";
constexpr std::string_view DiscAnimation = "my_DiskCh_b";

constexpr std::string_view FocusFrameLayout = "my_IplTop_d";
constexpr std::string_view FocusOnAnimation = "my_IplTop_d_FocusOn";
constexpr std::string_view FocusOffAnimation = "my_IplTop_d_FocusOff";
constexpr std::string_view RootPaneName = "RootPane";

// The groups of an icon that hold the words of one language each, and the group of the first animation of its script.
constexpr std::string_view EnglishGroup = "ENG";
constexpr std::array<std::string_view, 8> OtherLanguageGroups = {"JPN", "GER", "FRA", "SPA", "ITA", "NED", "KOR", "CHN"};
constexpr std::string_view ScriptGroup = "Rso0";

// The balloon hangs below the television of the place, which is 96 units high.
constexpr float HalfHeightOfAPlace = 48;

}

ChannelTile::ChannelTile(Display& menuDisplay, TileKind tileKind, std::unique_ptr<MenuLayout> pictureOfTheTile, const TileArchives& archives)
    : display(menuDisplay)
    , kind(tileKind)
    , picture(std::move(pictureOfTheTile))
    , focusFrame(std::make_unique<MenuLayout>(menuDisplay, archives.channelSelection, std::string(FocusFrameLayout)))
{
    focusOn = std::make_unique<FramePlayer>(*focusFrame, archives.channelSelection, FocusOnAnimation);
    focusOff = std::make_unique<FramePlayer>(*focusFrame, archives.channelSelection, FocusOffAnimation);
    lastFrameOfFocusOn = focusOn->LastFrameOfTheFile();
    lastFrameOfFocusOff = focusOff->LastFrameOfTheFile();
}

// The icons that a script drives get from the menu what the script shows while the channel has no data of its own:
// the panes of the language of the console and the first animation of the script (docs/research/Channels.md 1.6, CHN-03).
std::unique_ptr<ChannelTile> ChannelTile::MakeChannel(Display& display, const TileArchives& archives, const LoadedChannel& channel,
    const std::u16string& name)
{
    const Layout::LayoutArchive& icon = *channel.icon;
    auto picture = std::make_unique<MenuLayout>(display, icon, std::string(IconLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::Channel, std::move(picture), archives));
    if (channel.iconAnimation)
        tile->PlayAnimation(icon, *channel.iconAnimation);

    if (channel.isIconDrivenByCode)
        ShowOnlyTheEnglishPanes(*tile->picture, icon, std::string(IconLayout));

    if (channel.scriptAnimation)
        tile->PlayScriptAnimation(icon, *channel.scriptAnimation);

    tile->AddBalloon(archives, name);
    return tile;
}

std::unique_ptr<ChannelTile> ChannelTile::MakeDamagedChannel(Display& display, const TileArchives& archives)
{
    auto picture = std::make_unique<MenuLayout>(display, archives.damagedIcon, std::string(IconLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::DamagedChannel, std::move(picture), archives));
    tile->PlayAnimation(archives.damagedIcon, std::string(IconAnimation));
    return tile;
}

std::unique_ptr<ChannelTile> ChannelTile::MakeDiscChannel(Display& display, const TileArchives& archives, const std::u16string& name)
{
    auto picture = std::make_unique<MenuLayout>(display, archives.discThumbnail, std::string(DiscLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::DiscChannel, std::move(picture), archives));
    tile->PlayAnimation(archives.discThumbnail, std::string(DiscAnimation));
    tile->AddBalloon(archives, name);
    return tile;
}

std::unique_ptr<ChannelTile> ChannelTile::MakeEmptySlot(Display& display, const TileArchives& archives)
{
    auto picture = std::make_unique<MenuLayout>(display, archives.channelSelection, std::string(EmptySlotLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::EmptySlot, std::move(picture), archives));
    tile->PlayAnimation(archives.channelSelection, std::string(EmptySlotAnimation));
    return tile;
}

TileKind ChannelTile::Kind() const
{
    return kind;
}

// The frame grows into view when a pointer comes and fades away when it goes. Each animation plays once.
void ChannelTile::SetFocused(bool shouldBeFocused)
{
    if (shouldBeFocused == isFocused)
        return;

    isFocused = shouldBeFocused;
    if (isFocused)
        focusOn->PlayOnce(0, lastFrameOfFocusOn);
    else
        focusOff->PlayOnce(0, lastFrameOfFocusOff);

    if (balloon && isFocused)
        balloon->PointerArrived();
    else if (balloon)
        balloon->PointerLeft();
}

bool ChannelTile::IsFocused() const
{
    return isFocused;
}

const Balloon* ChannelTile::NameBalloon() const
{
    return balloon.get();
}

bool ChannelTile::Calculate()
{
    picture->Calculate();
    focusOn->Step();
    focusOff->Step();
    focusFrame->Calculate();
    if (!balloon)
        return false;

    bool wasBalloonShown = balloon->IsShown();
    balloon->Calculate();
    return !wasBalloonShown && balloon->IsShown();
}

void ChannelTile::DrawAt(Formats::Vector2 place)
{
    MoveLayoutTo(*picture, place);
    picture->Draw();

    MoveLayoutTo(*focusFrame, place);
    focusFrame->Draw();
}

void ChannelTile::DrawBalloonAt(Formats::Vector2 place)
{
    if (!balloon)
        return;

    balloon->PlaceBelow(place, HalfHeightOfAPlace);
    balloon->Draw();
}

// The original starts the animations of the tiles at a frame that depends on the time, so that the televisions do not flicker
// together. WeeOS starts them at frame 0.
void ChannelTile::PlayAnimation(const Layout::LayoutArchive& archive, const std::string& animationName)
{
    Layout::PlaybackMode mode = ModeOfAnimationFile(archive, animationName);
    std::size_t slot = picture->BindAnimationToPane(animationName, RootPaneName, mode);
    picture->StartAnimation(slot);
}

// The script plays its first animation on the group of the same name, Rso0.
void ChannelTile::PlayScriptAnimation(const Layout::LayoutArchive& archive, const std::string& animationName)
{
    Layout::PlaybackMode mode = ModeOfAnimationFile(archive, animationName);
    std::size_t slot = picture->BindAnimation(animationName, ScriptGroup, mode);
    picture->StartAnimation(slot);
}

void ChannelTile::AddBalloon(const TileArchives& archives, const std::u16string& name)
{
    if (name.empty())
        return;

    balloon = std::make_unique<Balloon>(display, archives.channelSelection);
    balloon->SetText(name);
}

// A layout of a channel holds its words in every language, each language in a group named after it. The panes of the other languages
// are hidden, except those that English uses as well.
void ShowOnlyTheEnglishPanes(MenuLayout& layout, const Layout::LayoutArchive& archive, const std::string& layoutName)
{
    Formats::Layout file = archive.ReadLayout(layoutName);
    if (!file.rootGroup)
        return;

    std::vector<std::string> englishPanes;
    for (const Formats::Group& group : file.rootGroup->children)
    {
        if (group.name == EnglishGroup)
            englishPanes = group.paneNames;
    }

    for (const Formats::Group& group : file.rootGroup->children)
    {
        bool isOtherLanguage = std::find(OtherLanguageGroups.begin(), OtherLanguageGroups.end(), group.name) != OtherLanguageGroups.end();
        if (!isOtherLanguage)
            continue;

        for (const std::string& paneName : group.paneNames)
        {
            bool isEnglishToo = std::find(englishPanes.begin(), englishPanes.end(), paneName) != englishPanes.end();
            if (!isEnglishToo)
                layout.PaneNamed(paneName).isVisible = false;
        }
    }
}

}
