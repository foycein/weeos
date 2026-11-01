#include "SystemMenu/WiiMenu/ChannelTile.h"

#include "SystemMenu/WiiMenu/LayoutParts.h"

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

// The balloon hangs below the television of the place, which is 96 units high.
constexpr float HalfHeightOfAPlace = 48;

constexpr Formats::Vector2 UnstretchedScale = {1, 1};

float LastFrameOf(const Layout::LayoutArchive& archive, std::string_view animationName)
{
    return static_cast<float>(archive.ReadAnimation(std::string(animationName)).frameCount - 1);
}

}

ChannelTile::ChannelTile(Display& menuDisplay, TileKind tileKind, std::unique_ptr<MenuLayout> pictureOfTheTile, const TileArchives& archives)
    : display(menuDisplay)
    , kind(tileKind)
    , picture(std::move(pictureOfTheTile))
    , focusFrame(std::make_unique<MenuLayout>(menuDisplay, archives.channelSelection, std::string(FocusFrameLayout)))
{
    focusOn = std::make_unique<FramePlayer>(*focusFrame, focusFrame->BindAnimationToAllPanes(std::string(FocusOnAnimation)));
    focusOff = std::make_unique<FramePlayer>(*focusFrame, focusFrame->BindAnimationToAllPanes(std::string(FocusOffAnimation)));
    lastFrameOfFocusOn = LastFrameOf(archives.channelSelection, FocusOnAnimation);
    lastFrameOfFocusOff = LastFrameOf(archives.channelSelection, FocusOffAnimation);
}

std::unique_ptr<ChannelTile> ChannelTile::MakeChannel(Display& display, const TileArchives& archives, const Layout::LayoutArchive& icon,
    const std::optional<std::string>& iconAnimation, const std::u16string& name)
{
    auto picture = std::make_unique<MenuLayout>(display, icon, std::string(IconLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::Channel, std::move(picture), archives));
    if (iconAnimation)
        tile->PlayAnimation(*iconAnimation);

    tile->AddBalloon(archives, name);
    return tile;
}

std::unique_ptr<ChannelTile> ChannelTile::MakeDamagedChannel(Display& display, const TileArchives& archives)
{
    auto picture = std::make_unique<MenuLayout>(display, archives.damagedIcon, std::string(IconLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::DamagedChannel, std::move(picture), archives));
    tile->PlayAnimation(std::string(IconAnimation));
    return tile;
}

std::unique_ptr<ChannelTile> ChannelTile::MakeDiscChannel(Display& display, const TileArchives& archives, const std::u16string& name)
{
    auto picture = std::make_unique<MenuLayout>(display, archives.discThumbnail, std::string(DiscLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::DiscChannel, std::move(picture), archives));
    tile->PlayAnimation(std::string(DiscAnimation));
    tile->AddBalloon(archives, name);
    return tile;
}

std::unique_ptr<ChannelTile> ChannelTile::MakeEmptySlot(Display& display, const TileArchives& archives)
{
    auto picture = std::make_unique<MenuLayout>(display, archives.channelSelection, std::string(EmptySlotLayout));
    std::unique_ptr<ChannelTile> tile(new ChannelTile(display, TileKind::EmptySlot, std::move(picture), archives));
    tile->PlayAnimation(std::string(EmptySlotAnimation));
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

void ChannelTile::Calculate()
{
    picture->Calculate();
    focusOn->Step();
    focusOff->Step();
    focusFrame->Calculate();
    if (balloon)
        balloon->Calculate();
}

void ChannelTile::DrawAt(Formats::Vector2 place)
{
    MoveLayoutTo(*picture, place);
    picture->PaneNamed(RootPaneName).scale = UnstretchedScale;
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

void ChannelTile::PlayAnimation(const std::string& animationName)
{
    std::size_t slot = picture->BindAnimationToAllPanes(animationName);
    picture->StartAnimation(slot);
}

void ChannelTile::AddBalloon(const TileArchives& archives, const std::u16string& name)
{
    if (name.empty())
        return;

    balloon = std::make_unique<Balloon>(display, archives.channelSelection);
    balloon->SetText(name);
}

}
