#include "SystemMenu/MessageBoard/MiiChooserScreen.h"

#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MessageBoard/BoardInput.h"
#include "SystemMenu/MessageBoard/MessageBoardData.h"
#include "SystemMenu/SoundRequests.h"

#include <array>
#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "faceSel";
constexpr std::string_view LayoutName = "my_SelectNigaoe_a";

constexpr std::string_view PageText = "T_Page";
constexpr std::string_view PageSlashText = "T_Page_01";
constexpr std::string_view PageCountText = "T_Page_02";
constexpr std::string_view NextPageButton = "B_ArwR";
constexpr std::string_view PreviousPageButton = "B_ArwL";

constexpr ButtonSet LeaveButtons = {Input::RemoteButtons::B, Input::ClassicButtons::B};

// The pages before and after the page that shows; they are only seen while the pages slide.
constexpr std::array<std::string_view, 2> NeighborPages = {"N_Nigaoe_01", "N_Nigaoe_02"};

std::u16string NumberText(int number)
{
    std::string digits = std::to_string(number);
    return std::u16string(digits.begin(), digits.end());
}

std::string PictureOfPlace(std::size_t place)
{
    return std::format("NigaoeSel{}", place);
}

std::string ShadowOfPlace(std::size_t place)
{
    return std::format("NigaoeSel{}_s0", place);
}

std::string ButtonOfPlace(std::size_t place)
{
    return std::format("B_NigaoeSel{}", place);
}

}

MiiChooserScreen::MiiChooserScreen(MenuServices& menuServices)
    : Screen(ScreenId::MiiChooser)
    , services(menuServices)
    , data(*menuServices.messageBoard)
{
}

void MiiChooserScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void MiiChooserScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));
    std::string layoutName(LayoutName);
    appearAnimation = layout->BindAnimationToAllPanes(layoutName + "_NigaoeIn");
    leaveAnimation = layout->BindAnimationToAllPanes(layoutName + "_NigaoeOut");
    arrowLoopAnimation = layout->BindAnimation(layoutName + "_Loop", "G_ArwRoop", Layout::PlaybackMode::Loop);
    layout->StartAnimation(appearAnimation);
    layout->StartAnimation(arrowLoopAnimation);

    for (std::string_view neighborPage : NeighborPages)
        layout->PaneNamed(neighborPage).isVisible = false;

    miis = MiiPortraits::MiisOfTheConsole(data.nand);
    ShowThePage();
    layout->Calculate();
}

int MiiChooserScreen::Page() const
{
    return page;
}

std::optional<Formats::Vector2> MiiChooserScreen::PlaceOfMii(std::size_t miiPlace)
{
    return layout->PointOfPaneInPicture(ButtonOfPlace(miiPlace), Formats::Vector2{0, 0});
}

int MiiChooserScreen::PageCount() const
{
    return static_cast<int>((miis.size() + MiisPerPage - 1) / MiisPerPage);
}

// The ten places of the page show the faces of their Miis; a place without a Mii is empty. The page number reads "1/3".
void MiiChooserScreen::ShowThePage()
{
    for (std::size_t place = 0; place < MiisPerPage; ++place)
    {
        std::size_t miiIndex = static_cast<std::size_t>(page) * MiisPerPage + place;
        std::optional<Layout::LoadedTexture> face;
        if (miiIndex < miis.size())
            face = data.portraits.FaceOf(miis[miiIndex]);

        Formats::Pane& picture = layout->PaneNamed(PictureOfPlace(place));
        picture.isVisible = face.has_value();
        layout->PaneNamed(ShadowOfPlace(place)).isVisible = face.has_value();
        layout->PaneNamed(ButtonOfPlace(place)).isVisible = face.has_value();
        if (face)
            layout->ShowTexture(PictureOfPlace(place), *face);
    }

    layout->SetText(PageText, NumberText(page + 1));
    layout->SetText(PageSlashText, u"/");
    layout->SetText(PageCountText, NumberText(std::max(1, PageCount())));
    layout->PaneNamed("N_ArwR").isVisible = page + 1 < PageCount();
    layout->PaneNamed("N_ArwL").isVisible = page > 0;
}

bool MiiChooserScreen::RunMain()
{
    layout->Calculate();
    if (isLeaving)
        return !layout->IsAnimationPlaying(leaveAnimation);

    if (!layout->IsAnimationPlaying(appearAnimation))
        RunChoosing();
    return false;
}

void MiiChooserScreen::RunChoosing()
{
    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        const std::optional<PlayerInput>& player = services.input.Player(playerIndex);
        std::optional<Formats::Vector2> place = PointerPlaceOf(services.input, services.display, playerIndex);
        bool isOnAMii = place && MiiPlaceAt(*place).has_value();
        if (isOnAMii && !wasOnAMii[playerIndex])
            services.sounds.Request(SoundName::ButtonTargeted);
        wasOnAMii[playerIndex] = isOnAMii;

        if (!player)
            continue;
        if (HasGoneDown(*player, LeaveButtons))
        {
            services.sounds.Request(SoundName::Cancel);
            Leave(std::nullopt);
            return;
        }
        if (!place || !HasGoneDown(*player, BoardPressButtons))
            continue;

        std::optional<std::size_t> miiPlace = MiiPlaceAt(*place);
        if (miiPlace)
        {
            services.sounds.Request(SoundName::Decide);
            Leave(miis[static_cast<std::size_t>(page) * MiisPerPage + *miiPlace]);
            return;
        }

        for (const Formats::Pane* button : layout->BoundingPanesAt(*place))
        {
            if (button->name == NextPageButton && page + 1 < PageCount())
            {
                TurnPage(1);
                return;
            }
            if (button->name == PreviousPageButton && page > 0)
            {
                TurnPage(-1);
                return;
            }
        }
    }
}

std::optional<std::size_t> MiiChooserScreen::MiiPlaceAt(Formats::Vector2 place)
{
    for (const Formats::Pane* button : layout->BoundingPanesAt(place))
    {
        for (std::size_t miiPlace = 0; miiPlace < MiisPerPage; ++miiPlace)
        {
            if (button->name == ButtonOfPlace(miiPlace) && button->isVisible)
                return miiPlace;
        }
    }
    return std::nullopt;
}

void MiiChooserScreen::TurnPage(int direction)
{
    services.sounds.Request(SoundName::ButtonPushed);
    page += direction;
    ShowThePage();
}

void MiiChooserScreen::Leave(std::optional<MiiData> chosenMii)
{
    data.chosenMii = chosenMii;
    layout->StartAnimation(leaveAnimation);
    isLeaving = true;
}

void MiiChooserScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main || !layout)
        return;

    layout->Draw();
}

}
