#include "SystemMenu/WiiMenu/ClockDisplay.h"

#include "SystemMenu/MenuClock.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"
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

constexpr std::string_view LabelPane = "T_WiiMenu";
constexpr std::string_view FirstHourDigitPane = "Clock3";
constexpr std::string_view SecondHourDigitPane = "Clock2";
constexpr std::string_view FirstMinuteDigitPane = "Clock1";
constexpr std::string_view SecondMinuteDigitPane = "Clock0";

// A console of the USA shows AM and PM on the right of the digits. Japan shows them on the left, Europe shows 24 hours.
constexpr std::string_view LeftAfternoonPane = "AM_PM";
constexpr std::string_view RightAfternoonPane = "AM_PM_R";

// The layout keeps a picture of every digit and of AM and PM, outside of the picture, for the clock to take the textures from.
constexpr std::array<std::string_view, 10> DigitPictures = {"Num0", "Num1", "Num2", "Num3", "Num4", "Num5", "Num6", "Num7", "Num8", "Num9"};
constexpr std::string_view MorningPicture = "AM";
constexpr std::string_view AfternoonPicture = "PM";

// The message "Wii Menu".
constexpr std::uint16_t LabelMessage = 16;

constexpr double SecondsOfTheLabel = 3;
constexpr int SecondsBetweenBlinks = 2;

bool IsBlinkSecond(const LocalTime& time)
{
    return time.second % SecondsBetweenBlinks == 0;
}

}

ClockDisplay::ClockDisplay(MenuServices& menuServices, const Layout::LayoutArchive& archive)
    : services(menuServices)
    , layout(std::make_unique<MenuLayout>(menuServices.display, archive, std::string(LayoutName)))
{
    change = std::make_unique<FramePlayer>(*layout, archive, ChangeAnimation);
    blink = std::make_unique<FramePlayer>(*layout, archive, BlinkAnimation);
    lastFrameOfTheChange = change->LastFrameOfTheFile();
    lastFrameOfTheBlink = blink->LastFrameOfTheFile();

    layout->PaneNamed(LeftAfternoonPane).isVisible = false;
    layout->SetText(LabelPane, services.messages.Find(LabelMessage));

    bool isFirstAppearance = !services.wiiMenu.hasClockShownTheLabel;
    services.wiiMenu.hasClockShownTheLabel = true;
    if (isFirstAppearance)
    {
        phase = Phase::ShowingTheLabel;
        labelStartedSeconds = services.clock.Seconds();
        change->Show(0);
    }
    else
    {
        phase = Phase::ShowingTheTime;
        change->Show(lastFrameOfTheChange);
    }

    ShowTheFace(MakeClockFace(services.wiiMenu.console.GetLocalTime()));
}

bool ClockDisplay::ShowsTheLabel() const
{
    return phase == Phase::ShowingTheLabel;
}

void ClockDisplay::Calculate()
{
    LocalTime time = services.wiiMenu.console.GetLocalTime();

    ClockFace face = MakeClockFace(time);
    if (face != shownFace)
        ShowTheFace(face);

    StartTheChangeWhenItIsTime(time);
    BlinkTheColon(time);
    change->Step();
    blink->Step();
    layout->Calculate();
}

void ClockDisplay::DrawAt(Formats::Vector2 place)
{
    MoveLayoutTo(*layout, place);
    layout->Draw();
}

void ClockDisplay::ShowTheFace(const ClockFace& face)
{
    CopyFirstTexture(*layout, FirstHourDigitPane, DigitPictures[face.hourTens]);
    CopyFirstTexture(*layout, SecondHourDigitPane, DigitPictures[face.hourOnes]);
    CopyFirstTexture(*layout, FirstMinuteDigitPane, DigitPictures[face.minuteTens]);
    CopyFirstTexture(*layout, SecondMinuteDigitPane, DigitPictures[face.minuteOnes]);
    CopyFirstTexture(*layout, RightAfternoonPane, face.isAfternoon ? AfternoonPicture : MorningPicture);

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
        if (hasShownLongEnough && IsBlinkSecond(time))
        {
            change->PlayOnce(0, lastFrameOfTheChange);
            phase = Phase::ChangingToTheTime;
        }
        return;
    }

    if (phase == Phase::ChangingToTheTime && !change->IsPlaying())
        phase = Phase::ShowingTheTime;
}

void ClockDisplay::BlinkTheColon(const LocalTime& time)
{
    if (phase == Phase::ShowingTheTime && !blink->IsPlaying() && IsBlinkSecond(time))
        blink->PlayOnce(0, lastFrameOfTheBlink);
}

}
