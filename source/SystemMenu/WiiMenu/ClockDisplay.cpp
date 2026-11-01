#include "SystemMenu/WiiMenu/ClockDisplay.h"

#include "SystemMenu/MenuClock.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"

#include "Text/MessageCatalog.h"

#include <array>
#include <string>

namespace SystemMenu
{

namespace
{

constexpr std::string_view LayoutName = "my_Clock_a";
constexpr std::string_view ChangeAnimation = "my_Clock_a_Change";
constexpr std::string_view BlinkAnimation = "my_Clock_a_Min";

constexpr std::string_view ClockPane = "N_WiiMenu";
constexpr std::string_view LabelPane = "T_WiiMenu";
constexpr std::string_view ColonPane = "ClockTen";
constexpr std::string_view FirstHourDigitPane = "Clock3";
constexpr std::string_view SecondHourDigitPane = "Clock2";
constexpr std::string_view FirstMinuteDigitPane = "Clock1";
constexpr std::string_view SecondMinuteDigitPane = "Clock0";
constexpr std::string_view AfternoonPane = "AM_PM";
constexpr std::string_view AfternoonOnTheRightPane = "AM_PM_R";

// The layout keeps a picture of every digit and of AM and PM for the clock to take the textures from.
constexpr std::array<std::string_view, 10> DigitPictures = {"Num0", "Num1", "Num2", "Num3", "Num4", "Num5", "Num6", "Num7", "Num8", "Num9"};
constexpr std::string_view MorningPicture = "AM";
constexpr std::string_view AfternoonPicture = "PM";

// The message "Wii Menu".
constexpr std::uint16_t LabelMessage = 16;

// The change from the label to the time has 26 frames.
constexpr float LastFrameOfTheChange = 25;

constexpr double SecondsOfTheLabel = 3;
constexpr int SecondsBetweenBlinks = 2;

bool IsEvenSecond(const LocalTime& time)
{
    return time.second % SecondsBetweenBlinks == 0;
}

}

ClockDisplay::ClockDisplay(MenuServices& menuServices, const Layout::LayoutArchive& archive)
    : services(menuServices)
    , layout(std::make_unique<MenuLayout>(menuServices.display, archive, std::string(LayoutName)))
{
    changeSlot = layout->BindAnimationToWholeLayout(std::string(ChangeAnimation), Layout::PlaybackMode::PlayOnce);
    blinkSlot = layout->BindAnimationToPane(std::string(BlinkAnimation), ColonPane, Layout::PlaybackMode::PlayOnce);

    // A console of the USA shows AM and PM on the left of the digits.
    layout->PaneNamed(AfternoonOnTheRightPane).isVisible = false;
    layout->SetText(LabelPane, services.messages.Find(LabelMessage));

    bool isFirstAppearance = !services.wiiMenu.hasClockShownTheLabel;
    services.wiiMenu.hasClockShownTheLabel = true;
    if (isFirstAppearance)
    {
        phase = Phase::ShowingTheLabel;
        labelStartedSeconds = services.clock.Seconds();
        layout->ShowAnimationFrame(changeSlot, 0);
    }
    else
    {
        phase = Phase::ShowingTheTime;
        layout->ShowAnimationFrame(changeSlot, LastFrameOfTheChange);
    }

    for (const Formats::Pane* pane : Layout::PaneWithDescendants(layout->PaneNamed(ClockPane)))
        panesOfTheClock.insert(pane);

    ShowTheFace(MakeClockFace(services.wiiMenu.console.GetLocalTime()));
}

void ClockDisplay::Calculate()
{
    LocalTime time = services.wiiMenu.console.GetLocalTime();

    ClockFace face = MakeClockFace(time);
    if (face != shownFace)
        ShowTheFace(face);

    StartTheChangeWhenItIsTime(time);
    BlinkTheColon(time);
    layout->Calculate();
}

void ClockDisplay::DrawAt(Formats::Vector2 place)
{
    layout->MoveTo(place);
    layout->Draw([&](const Formats::Pane& pane) { return panesOfTheClock.contains(&pane); });
}

void ClockDisplay::ShowTheFace(const ClockFace& face)
{
    layout->CopyTexture(FirstHourDigitPane, DigitPictures[face.hourTens]);
    layout->CopyTexture(SecondHourDigitPane, DigitPictures[face.hourOnes]);
    layout->CopyTexture(FirstMinuteDigitPane, DigitPictures[face.minuteTens]);
    layout->CopyTexture(SecondMinuteDigitPane, DigitPictures[face.minuteOnes]);
    layout->CopyTexture(AfternoonPane, face.isAfternoon ? AfternoonPicture : MorningPicture);

    // A clock does not show a 0 in front of the hours.
    layout->PaneNamed(FirstHourDigitPane).isVisible = face.hourTens != 0;
    shownFace = face;
}

// The label changes to the time on a second that the colon would blink on, so that the two do not start at different times.
void ClockDisplay::StartTheChangeWhenItIsTime(const LocalTime& time)
{
    if (phase == Phase::ShowingTheLabel)
    {
        bool hasShownLongEnough = services.clock.Seconds() - labelStartedSeconds >= SecondsOfTheLabel;
        if (hasShownLongEnough && IsEvenSecond(time))
        {
            layout->StartAnimation(changeSlot);
            phase = Phase::ChangingToTheTime;
        }
    }
    else if (phase == Phase::ChangingToTheTime && !layout->IsAnimationPlaying(changeSlot))
    {
        phase = Phase::ShowingTheTime;
    }
}

// The colon goes out and comes back once for every two seconds. The animation takes a little more than a second.
void ClockDisplay::BlinkTheColon(const LocalTime& time)
{
    if (phase == Phase::ShowingTheTime && !layout->IsAnimationPlaying(blinkSlot) && IsEvenSecond(time))
        layout->StartAnimation(blinkSlot);
}

}
