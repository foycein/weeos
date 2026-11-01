#include "SystemMenu/WiiSettings/SensorBarPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

constexpr int ChoiceCount = 2;
constexpr int LowestSensitivity = 1;
constexpr int HighestSensitivity = 5;

// The gauge of the sensitivity: a bar with a round button at each end, and a marker that stands at the level that is set.
// The marker pictures are 56 by 56 points, one for each level, and they are 72 points apart.
constexpr PageRectangle GaugeBounds = {156, 296, 296, 48};
constexpr PageRectangle MinusBounds = {90, 304, 32, 32};
constexpr PageRectangle PlusBounds = {480, 304, 32, 32};
constexpr int FirstMarkerLeft = 132;
constexpr int MarkerDistance = 72;
constexpr int MarkerTop = 300;
constexpr int MarkerSize = 56;

constexpr PageRectangle HintBounds = {24, 130, 560, 120};

std::string MarkerPicturePath(int sensitivity)
{
    return "FIX/COMMON/BTN/DPD_Rank0" + std::to_string(sensitivity) + ".gif";
}

}

SensorBarPages::SensorBarPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings)
    : navigator(pageNavigator)
    , settings(systemSettings)
{
}

bool SensorBarPages::Owns(PageId page) const
{
    switch (page)
    {
    case PageId::SensorBarIndex:
    case PageId::SensorBarPositionSet:
    case PageId::SensitivityIntroduction:
    case PageId::SensitivitySet:
        return true;

    default:
        return false;
    }
}

void SensorBarPages::EnterPage(PageId page)
{
    if (page == PageId::SensorBarPositionSet)
        positionAtEntry = settings.GetSensorBarPosition();
    else if (page == PageId::SensitivitySet)
        sensitivityAtEntry = settings.GetPointerSensitivity();
}

void SensorBarPages::BuildPage(PageId page, Page& content)
{
    switch (page)
    {
    case PageId::SensorBarIndex:
        BuildSensorBarIndex(content);
        break;

    case PageId::SensorBarPositionSet:
        BuildPositionSet(content);
        break;

    case PageId::SensitivityIntroduction:
        BuildSensitivityIntroduction(content);
        break;

    default:
        BuildSensitivitySet(content);
        break;
    }
}

void SensorBarPages::BuildSensorBarIndex(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, u"Sensor Bar");

    builder.AddChoice(0, ChoiceCount, u"Sensor Bar Position", false, [this] { navigator.GoTo(PageId::SensorBarPositionSet); })
        .sound = PageSound::Decide;
    builder.AddChoice(1, ChoiceCount, u"Sensitivity", false, [this] { navigator.GoTo(PageId::SensitivityIntroduction); })
        .sound = PageSound::Decide;
    builder.AddBackButton([this] { navigator.GoTo(PageId::Index); });
}

void SensorBarPages::BuildPositionSet(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, u"Sensor Bar Position");

    Settings::SensorBarPosition position = settings.GetSensorBarPosition();
    builder.AddChoice(0, ChoiceCount, u"Above TV", position == Settings::SensorBarPosition::AboveTelevision,
        [this] { ChoosePosition(Settings::SensorBarPosition::AboveTelevision); });
    builder.AddChoice(1, ChoiceCount, u"Below TV", position == Settings::SensorBarPosition::BelowTelevision,
        [this] { ChoosePosition(Settings::SensorBarPosition::BelowTelevision); });

    builder.AddBackButton(
        [this]
        {
            settings.SetSensorBarPosition(positionAtEntry);
            navigator.GoTo(PageId::SensorBarIndex);
        });
    builder.AddBottomRight(u"Confirm",
        [this]
        {
            settings.Save();
            navigator.GoTo(PageId::SensorBarIndex);
        });
}

void SensorBarPages::BuildSensitivityIntroduction(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, u"Sensitivity");

    builder.AddText(HintBounds, TextRole::Message,
        {u"The pointing device can't be used", u"to navigate until sensitivity is set.", u"Press 1 and 2 on the Wii",
            u"Remote to set sensitivity,", u"then press A. Check the Wii", u"Operations Manual for details."});

    builder.AddBackButton([this] { navigator.GoTo(PageId::SensorBarIndex); });
    builder.AddBottomRight(u"OK", [this] { navigator.GoTo(PageId::SensitivitySet); });
}

void SensorBarPages::BuildSensitivitySet(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(2, u"Sensitivity");

    int sensitivity = settings.GetPointerSensitivity();
    builder.AddPicture("FIX/COMMON/BG/DPD_PMGauge.gif", GaugeBounds);
    builder.AddPicture(MarkerPicturePath(sensitivity),
        {FirstMarkerLeft + (sensitivity - LowestSensitivity) * MarkerDistance, MarkerTop, MarkerSize, MarkerSize});

    PageButton& minus = builder.AddPictureButton(ButtonLook::MinusCircle, MinusBounds, "Lower", [this] { ChangeSensitivity(-1); });
    minus.sound = sensitivity > LowestSensitivity ? PageSound::Choose : PageSound::Error;
    PageButton& plus = builder.AddPictureButton(ButtonLook::PlusCircle, PlusBounds, "Higher", [this] { ChangeSensitivity(1); });
    plus.sound = sensitivity < HighestSensitivity ? PageSound::Choose : PageSound::Error;

    builder.AddText(HintBounds, TextRole::Message, {u"Press A when done."});

    builder.AddBackButton(
        [this]
        {
            settings.SetPointerSensitivity(sensitivityAtEntry);
            navigator.GoTo(PageId::SensorBarIndex);
        });
    builder.AddBottomRight(u"OK",
        [this]
        {
            settings.Save();
            navigator.GoTo(PageId::SensorBarIndex);
        });
}

void SensorBarPages::ChoosePosition(Settings::SensorBarPosition position)
{
    settings.SetSensorBarPosition(position);
    navigator.Refresh();
}

void SensorBarPages::ChangeSensitivity(int change)
{
    int sensitivity = std::clamp(settings.GetPointerSensitivity() + change, LowestSensitivity, HighestSensitivity);
    settings.SetPointerSensitivity(sensitivity);
    navigator.Refresh();
}

}
