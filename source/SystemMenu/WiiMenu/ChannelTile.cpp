#include "SystemMenu/WiiMenu/ChannelTile.h"

#include "SystemMenu/WiiMenu/LayoutParts.h"

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

// The groups of the languages that a console of the USA looks for, in this order. The menu speaks English.
constexpr std::array<std::string_view, 3> LanguagesInTheOrderOfTheUsa = {"ENG", "FRA", "SPA"};

// The balloon hangs below the television of the place, which is 96 units high, 20 steps after the pointer came (0x816949E0).
constexpr float HalfHeightOfAPlace = 48;
constexpr int StepsBeforeTheBalloonAppears = 20;

void SetPanesVisible(Layout::LiveLayout& layout, const std::vector<std::string>& paneNames, bool isVisible)
{
    for (const std::string& paneName : paneNames)
    {
        Formats::Pane* pane = layout.FindPane(paneName);
        if (pane != nullptr)
            pane->isVisible = isVisible;
    }
}

// The groups Rso0 to Rso15 belong to the animations of a script.
bool IsGroupOfAScript(std::string_view groupName)
{
    for (int number = 0; number < ScriptAnimationCount; ++number)
    {
        if (groupName == ScriptGroupName(number))
            return true;
    }
    return false;
}

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

// Every icon shows the panes of its language (0x813B2B18 calls 0x813B24C0). An icon that a script drives then gets what its script
// does, run once when the tile is made.
std::unique_ptr<ChannelTile> ChannelTile::MakeChannel(Display& display, const TileArchives& archives, const LoadedChannel& channel,
    const std::u16string& name, std::uint32_t randomNumber)
{
    const Layout::LayoutArchive& icon = *channel.icon;
    std::unique_ptr<Layout::PictureSource> postedPhotoPictures;
    std::unique_ptr<MenuLayout> picture;
    if (channel.postedPhoto)
    {
        postedPhotoPictures = std::make_unique<PostedPhotoPictures>(icon, *channel.postedPhoto);
        picture = std::make_unique<MenuLayout>(display, icon, std::string(IconLayout), *postedPhotoPictures);
    }
    else
    {
        picture = std::make_unique<MenuLayout>(display, icon, std::string(IconLayout));
    }

    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::Channel, std::move(picture), archives));
    tile->postedPhotoPictures = std::move(postedPhotoPictures);
    ShowOnlyTheEnglishPanes(tile->picture->Live(), icon.ReadLayout(std::string(IconLayout)));

    std::uint32_t randomNumberOfTheIcon = channel.isIconDrivenByCode ? NoRandomNumber : randomNumber;
    if (channel.iconAnimation)
        tile->PlayAnimation(icon, *channel.iconAnimation, randomNumberOfTheIcon);

    if (channel.isIconDrivenByCode)
    {
        tile->scriptAnimations = std::make_unique<ScriptAnimations>(*tile->picture, icon, channel.scriptAnimationNumbers);
        RunIconScript(channel.iconScript, *tile->picture, *tile->scriptAnimations, channel.postedPhoto.has_value());
    }

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

    if (scriptAnimations)
        scriptAnimations->Step();

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

// The menu hides the panes of every group except the groups of the script, Rso0 to Rso15, and then shows the panes of the group of
// its language again, so a pane that two languages share stays shown. Without an English group the first of French and Spanish
// shows, the order of a console of the USA (0x813B24C0 with the table 0x8164E368).
void ShowOnlyTheEnglishPanes(Layout::LiveLayout& layout, const Formats::Layout& file)
{
    if (!file.rootGroup)
        return;

    for (const Formats::Group& group : file.rootGroup->children)
    {
        if (IsGroupOfAScript(group.name))
            continue;

        SetPanesVisible(layout, group.paneNames, false);
    }

    for (std::string_view languageName : LanguagesInTheOrderOfTheUsa)
    {
        const Formats::Group* languageGroup = FindTopGroup(file, languageName);
        if (!languageGroup)
            continue;

        SetPanesVisible(layout, languageGroup->paneNames, true);
        return;
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
