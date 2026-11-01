#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/WiiMenu/ClockFace.h"

#include <memory>
#include <optional>
#include <set>

namespace SystemMenu
{

// The clock at the bottom of the Wii Menu (the layout my_Clock_a). The first time the Wii Menu appears it shows the words "Wii Menu"
// for a few seconds and then changes to the time. The colon blinks every two seconds.
class ClockDisplay
{
public:
    // The archive holds the layout my_Clock_a and its animations. The services and the archive must outlive the clock.
    ClockDisplay(MenuServices& menuServices, const Layout::LayoutArchive& archive);

    void Calculate();

    // Draws the clock with its middle at a place of the picture. The grid draws it in the notch of every page.
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
    std::size_t changeSlot = 0;
    std::size_t blinkSlot = 0;
    Phase phase = Phase::ShowingTheTime;
    double labelStartedSeconds = 0;
    std::optional<ClockFace> shownFace;
    std::set<const Formats::Pane*> panesOfTheClock;
};

}
