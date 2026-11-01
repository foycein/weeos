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
constexpr std::string_view SelectAnimation = "my_IplTop_d_Select";

// The animations of the disc and of a damaged channel, and the icons that a script drives, start at their first frame.
constexpr std::uint32_t NoRandomNumber = 0;

// The groups of an icon that hold the words of one language each, and the group of the first animation of its script.
constexpr std::string_view EnglishGroup = "ENG";
constexpr std::array<std::string_view, 8> OtherLanguageGroups = {"JPN", "GER", "FRA", "SPA", "ITA", "NED", "KOR", "CHN"};
constexpr std::string_view ScriptGroup = "Rso0";

// The balloon hangs below the television of the place, which is 96 units high, 20 steps after the pointer came (0x816949E0).
constexpr float HalfHeightOfAPlace = 48;
constexpr int StepsBeforeTheBalloonAppears = 20;

}

ChannelTile::ChannelTile(Display& menuDisplay, TileKind tileKind, std::unique_ptr<MenuLayout> pictureOfTheTile, const TileArchives& archives)
    : display(menuDisplay)
    , kind(tileKind)
    , picture(std::move(pictureOfTheTile))
    , focusFrame(std::make_unique<MenuLayout>(menuDisplay, archives.channelSelection, std::string(FocusFrameLayout)))
{
    focusOn = std::make_unique<FramePlayer>(*focusFrame, archives.channelSelection, FocusOnAnimation);
    focusOff = std::make_unique<FramePlayer>(*focusFrame, archives.channelSelection, FocusOffAnimation);
    select = std::make_unique<FramePlayer>(*focusFrame, archives.channelSelection, SelectAnimation);
}

// The icons that a script drives get from the menu what the script shows while the channel has no data of its own:
// the panes of the language of the console and the first animation of the script (docs/research/Channels.md 1.6, CHN-03).
std::unique_ptr<ChannelTile> ChannelTile::MakeChannel(Display& display, const TileArchives& archives, const LoadedChannel& channel,
    const std::u16string& name, std::uint32_t randomNumber)
{
    const Layout::LayoutArchive& icon = *channel.icon;
    auto picture = std::make_unique<MenuLayout>(display, icon, std::string(IconLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::Channel, std::move(picture), archives));
    std::uint32_t randomNumberOfTheIcon = channel.isIconDrivenByCode ? NoRandomNumber : randomNumber;
    if (channel.iconAnimation)
        tile->PlayAnimation(icon, *channel.iconAnimation, randomNumberOfTheIcon);

    if (channel.isIconDrivenByCode)
        tile->ShowOnlyTheEnglishPanes(icon);

    if (channel.scriptAnimation)
        tile->PlayScriptAnimation(icon, *channel.scriptAnimation);

    tile->AddBalloon(archives, name);
    return tile;
}

std::unique_ptr<ChannelTile> ChannelTile::MakeDamagedChannel(Display& display, const TileArchives& archives)
{
    auto picture = std::make_unique<MenuLayout>(display, archives.damagedIcon, std::string(IconLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::DamagedChannel, std::move(picture), archives));
    tile->PlayAnimation(archives.damagedIcon, std::string(IconAnimation), NoRandomNumber);
    return tile;
}

std::unique_ptr<ChannelTile> ChannelTile::MakeDiscChannel(Display& display, const TileArchives& archives, const std::u16string& name)
{
    auto picture = std::make_unique<MenuLayout>(display, archives.discThumbnail, std::string(DiscLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::DiscChannel, std::move(picture), archives));
    tile->PlayAnimation(archives.discThumbnail, std::string(DiscAnimation), NoRandomNumber);
    tile->AddBalloon(archives, name);
    return tile;
}

std::unique_ptr<ChannelTile> ChannelTile::MakeEmptySlot(Display& display, const TileArchives& archives, std::uint32_t randomNumber)
{
    auto picture = std::make_unique<MenuLayout>(display, archives.channelSelection, std::string(EmptySlotLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::EmptySlot, std::move(picture), archives));
    tile->PlayAnimation(archives.channelSelection, std::string(EmptySlotAnimation), randomNumber);
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
        focusOn->PlayOnce(0, focusOn->LastFrameOfTheFile());
    else
        focusOff->PlayOnce(0, focusOff->LastFrameOfTheFile());

    if (balloon && isFocused)
        balloon->PointerArrived();
    else if (balloon)
        balloon->PointerLeft();
}

bool ChannelTile::IsFocused() const
{
    return isFocused;
}

void ChannelTile::ShowPressed()
{
    select->PlayOnce(0, select->LastFrameOfTheFile());
}

float ChannelTile::FrameOfThePicture() const
{
    return pictureAnimation ? pictureAnimation->Frame() : 0;
}

const Balloon* ChannelTile::NameBalloon() const
{
    return balloon.get();
}

bool ChannelTile::Calculate()
{
    if (pictureAnimation)
        pictureAnimation->Step();

    if (scriptAnimation)
        scriptAnimation->Step();

    picture->Calculate();
    focusOn->Step();
    focusOff->Step();
    select->Step();
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

// A loop starts at the random number modulo the length of the animation (0x813B2B18, 0x813B2E8C); an animation that plays once starts at its first frame.
void ChannelTile::PlayAnimation(const Layout::LayoutArchive& archive, const std::string& animationName, std::uint32_t randomNumber)
{
    pictureAnimation = std::make_unique<FramePlayer>(*picture, archive, animationName);
    float lastFrame = pictureAnimation->LastFrameOfTheFile();
    if (ModeOfAnimationFile(archive, animationName) == Layout::PlaybackMode::PlayOnce)
    {
        pictureAnimation->PlayOnce(0, lastFrame);
        return;
    }

    std::uint32_t frameCount = static_cast<std::uint32_t>(lastFrame) + 1;
    float startFrame = static_cast<float>(randomNumber % frameCount);
    pictureAnimation->LoopFrom(0, lastFrame + 1, startFrame);
}

// The script plays its first animation on the group of the same name, Rso0.
void ChannelTile::PlayScriptAnimation(const Layout::LayoutArchive& archive, const std::string& animationName)
{
    scriptAnimation = std::make_unique<FramePlayer>(*picture, archive, animationName, picture->Live().PanesOfGroup(ScriptGroup));
    scriptAnimation->Loop(0, scriptAnimation->LastFrameOfTheFile() + 1);
}

// An icon holds its words in every language, each language in a group named after it. The panes of the other languages are hidden,
// except those that English uses as well.
void ChannelTile::ShowOnlyTheEnglishPanes(const Layout::LayoutArchive& archive)
{
    Formats::Layout file = archive.ReadLayout(std::string(IconLayout));
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
                picture->PaneNamed(paneName).isVisible = false;
        }
    }
}

void ChannelTile::AddBalloon(const TileArchives& archives, const std::u16string& name)
{
    if (name.empty())
        return;

    balloon = std::make_unique<Balloon>(display, archives.channelSelection, StepsBeforeTheBalloonAppears);
    balloon->SetText(name);
}

}
