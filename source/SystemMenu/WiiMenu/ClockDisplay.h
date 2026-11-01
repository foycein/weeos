#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/WiiMenu/ClockFace.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include <memory>
#include <optional>

namespace SystemMenu
{

// The clock below the channels of the Wii Menu (the layout my_Clock_a). The first time the Wii Menu appears it shows the words
// "Wii Menu" for three seconds and then changes to the time. The colon goes out and comes back every two seconds.
class ClockDisplay
{
public:
    // The archive holds the layout my_Clock_a and its animations. The services and the archive must outlive the clock.
    ClockDisplay(MenuServices& menuServices, const Layout::LayoutArchive& archive);

    // True while the clock shows the words "Wii Menu" and not the time.
    bool ShowsTheLabel() const;

    void Calculate();

    // Draws the clock with its middle at a place of the picture. The grid draws it below the channels of every page that shows.
    void DrawAt(Formats::Vector2 place);

private:
    enum class Phase
    {
        ShowingTheLabel,
        ChangingToTheTime,
        ShowingTheTime,
    };

    void ShowTheFace(const ClockFace& face);
    void StartTheChangeWhenItIsTime(const LocalTime& time);
    void BlinkTheColon(const LocalTime& time);

    MenuServices& services;
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<FramePlayer> change;
    std::unique_ptr<FramePlayer> blink;
    float lastFrameOfTheChange = 0;
    float lastFrameOfTheBlink = 0;
    Phase phase = Phase::ShowingTheTime;
    double labelStartedSeconds = 0;
    std::optional<ClockFace> shownFace;
};

}
