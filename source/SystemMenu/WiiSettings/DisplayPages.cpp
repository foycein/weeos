#include "SystemMenu/WiiSettings/DisplayPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"
#include "SystemMenu/WiiSettings/NumberText.h"

#include <cstdlib>

namespace SystemMenu
{

namespace
{

// The page moves the picture by 2 units at a time and by 16 at most, to the left and to the right. The page shows the number of steps.
constexpr int DisplayOffsetStep = 2;
constexpr int LargestOffsetOfThePage = 16;

constexpr PageRectangle OffsetArrowLeftBounds = {160, 180, 72, 72};
constexpr PageRectangle OffsetArrowRightBounds = {376, 180, 72, 72};
constexpr PageRectangle OffsetNumberBounds = {232, 168, 144, 96};

constexpr PageRectangle StandardTelevisionBounds = {48, 146, 200, 140};
constexpr PageRectangle WideTelevisionBounds = {296, 146, 264, 140};
constexpr PageRectangle StandardLabelBounds = {48, 300, 200, 40};
constexpr PageRectangle WideLabelBounds = {296, 300, 264, 40};

std::u16string OffsetText(int offset)
{
    int steps = offset / DisplayOffsetStep;
    if (steps > 0)
        return u"+" + NumberText(steps);

    if (steps < 0)
        return u"-" + NumberText(-steps);

    return u"0";
}

}

DisplayPages::DisplayPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings)
    : navigator(pageNavigator)
    , settings(systemSettings)
{
}

bool DisplayPages::Owns(PageId page) const
{
    switch (page)
    {
    case PageId::DisplayIndex:
    case PageId::ScreenPosition:
    case PageId::WidescreenSet:
    case PageId::TvResolution:
    case PageId::BurnInReduction:
        return true;

    default:
        return false;
    }
}

void DisplayPages::EnterPage(PageId page)
{
    if (page == PageId::ScreenPosition)
        displayOffsetAtEntry = settings.GetDisplayOffset();
    else if (page == PageId::WidescreenSet)
        aspectRatioAtEntry = settings.GetAspectRatio();
    else if (page == PageId::TvResolution)
        isProgressiveChosen = settings.IsProgressiveScanOn();
    else if (page == PageId::BurnInReduction)
        isScreenSaverChosen = settings.IsScreenSaverOn();
}

void DisplayPages::BuildPage(PageId page, Page& content)
{
    switch (page)
    {
    case PageId::DisplayIndex:
        BuildDisplayIndex(content);
        break;

    case PageId::ScreenPosition:
        BuildScreenPosition(content);
        break;

    case PageId::WidescreenSet:
        BuildWidescreenSet(content);
        break;

    case PageId::TvResolution:
        BuildTvResolution(content);
        break;

    default:
        BuildBurnInReduction(content);
        break;
    }
}

void DisplayPages::BuildDisplayIndex(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, u"Screen");

    builder.AddMenuItem(0, u"Screen Position", [this] { navigator.GoTo(PageId::ScreenPosition); });
    builder.AddMenuItem(1, u"Widescreen Settings", [this] { navigator.GoTo(PageId::WidescreenSet); });
    builder.AddMenuItem(2, u"TV Resolution", [this] { navigator.GoTo(PageId::TvResolution); });
    builder.AddMenuItem(3, u"Screen Burn-in Reduction", [this] { navigator.GoTo(PageId::BurnInReduction); });
    builder.AddBackButton([this] { navigator.GoTo(PageId::FirstIndex); });
}

// The left arrow moves the picture to the left. A move beyond the largest offset is refused with the sound of an error.
void DisplayPages::BuildScreenPosition(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, u"Screen Position");

    PageButton& left = builder.AddStepArrow(ButtonLook::ArrowLeft, OffsetArrowLeftBounds.left, OffsetArrowLeftBounds.top, "MoveLeft",
        [this] { MovePicture(-DisplayOffsetStep); });
    PageButton& right = builder.AddStepArrow(ButtonLook::ArrowRight, OffsetArrowRightBounds.left, OffsetArrowRightBounds.top, "MoveRight",
        [this] { MovePicture(DisplayOffsetStep); });

    int offset = settings.GetDisplayOffset();
    left.sound = offset <= -LargestOffsetOfThePage ? PageSound::Error : PageSound::Choose;
    right.sound = offset >= LargestOffsetOfThePage ? PageSound::Error : PageSound::Choose;

    builder.AddText(OffsetNumberBounds, TextRole::BigNumber, {OffsetText(offset)});
    builder.AddBackButton([this] { LeaveScreenPositionWithoutConfirming(); });
    builder.AddBottomRight(u"Confirm", [this] { SaveAndGoBackToTheDisplayIndex(); });
}

void DisplayPages::BuildWidescreenSet(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, u"Widescreen Settings");

    bool isWide = settings.GetAspectRatio() == Settings::AspectRatio::SixteenByNine;

    builder.AddPictureButton(ButtonLook::TelevisionFourByThree, StandardTelevisionBounds, "Standard",
        [this] { ChooseAspectRatio(Settings::AspectRatio::FourByThree); }).isChosen = !isWide;
    builder.AddPictureButton(ButtonLook::TelevisionSixteenByNine, WideTelevisionBounds, "Widescreen",
        [this] { ChooseAspectRatio(Settings::AspectRatio::SixteenByNine); }).isChosen = isWide;

    builder.AddText(StandardLabelBounds, TextRole::Message, {u"Standard (4:3)"});
    builder.AddText(WideLabelBounds, TextRole::Message, {u"Widescreen (16:9)"});

    builder.AddBackButton([this] { LeaveWidescreenWithoutConfirming(); });
    builder.AddBottomRight(u"Confirm", [this] { SaveAndGoBackToTheDisplayIndex(); });
}

void DisplayPages::BuildTvResolution(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, u"TV Resolution");

    builder.AddChoice(0, 2, u"EDTV or HDTV (480p)", isProgressiveChosen,
        [this]
        {
            isProgressiveChosen = true;
            navigator.Refresh();
        });
    builder.AddChoice(1, 2, u"Standard TV (480i)", !isProgressiveChosen,
        [this]
        {
            isProgressiveChosen = false;
            navigator.Refresh();
        });

    builder.AddBackButton([this] { navigator.GoTo(PageId::DisplayIndex); });
    builder.AddBottomRight(u"Confirm",
        [this]
        {
            settings.SetProgressiveScan(isProgressiveChosen);
            settings.Save();
            navigator.GoTo(PageId::DisplayIndex);
        });
}

void DisplayPages::BuildBurnInReduction(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, u"Screen Burn-in Reduction");

    builder.AddChoice(0, 2, u"On", isScreenSaverChosen,
        [this]
        {
            isScreenSaverChosen = true;
            navigator.Refresh();
        });
    builder.AddChoice(1, 2, u"Off", !isScreenSaverChosen,
        [this]
        {
            isScreenSaverChosen = false;
            navigator.Refresh();
        });

    builder.AddBackButton([this] { navigator.GoTo(PageId::DisplayIndex); });
    builder.AddBottomRight(u"Confirm",
        [this]
        {
            settings.SetScreenSaver(isScreenSaverChosen);
            settings.Save();
            navigator.GoTo(PageId::DisplayIndex);
        });
}

void DisplayPages::MovePicture(int change)
{
    int offset = settings.GetDisplayOffset() + change;
    if (std::abs(offset) <= LargestOffsetOfThePage)
        settings.SetDisplayOffset(offset);

    navigator.Refresh();
}

void DisplayPages::ChooseAspectRatio(Settings::AspectRatio aspectRatio)
{
    settings.SetAspectRatio(aspectRatio);
    navigator.Refresh();
}

void DisplayPages::LeaveScreenPositionWithoutConfirming()
{
    settings.SetDisplayOffset(displayOffsetAtEntry);
    navigator.GoTo(PageId::DisplayIndex);
}

void DisplayPages::LeaveWidescreenWithoutConfirming()
{
    settings.SetAspectRatio(aspectRatioAtEntry);
    navigator.GoTo(PageId::DisplayIndex);
}

void DisplayPages::SaveAndGoBackToTheDisplayIndex()
{
    settings.Save();
    navigator.GoTo(PageId::DisplayIndex);
}

}
