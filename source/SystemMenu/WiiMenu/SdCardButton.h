#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/WiiMenu/Balloon.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include "Formats/Vectors.h"
#include "Layout/LayoutArchive.h"

#include <memory>
#include <string>

namespace SystemMenu
{

// The SD card button of the Wii Menu (the layout mn_Sdcard_Btn of cmnBtn.ash, the object +0x70 of the bar in the original,
// 0x813EAA18 to 0x813EB51C). It sits at the left of the bar, grey while the slot is empty and white while a card is in it,
// and leads to the SD Card Menu.
class SdCardButton
{
public:
    // The archives hold mn_Sdcard_Btn and my_IplTopBalloon_a. Everything given must outlive the button.
    SdCardButton(Display& display, const Layout::LayoutArchive& buttonArchive, const Layout::LayoutArchive& balloonArchive,
        const std::u16string& balloonText);

    // The button grows into view (BtnL_Out played backwards, the bar event 0x28).
    void Appear();

    // White while a card is in the slot, grey while it is empty (0x813EB330).
    void ShowCardInserted(bool isInserted);

    // A card came: the ring around the button flashes once (the bar event 0x2A).
    void PlayInsert();

    // A hand came onto the button or went: it grows a little and, after a moment, shows its balloon.
    void SetFocused(bool shouldBeFocused);
    bool IsFocused() const;
    const Balloon& NameBalloon() const;

    // The button was pressed (the bar event 0x2B).
    void PlayPress();

    // True when the place of the picture lies on the area of the button.
    bool IsAt(Formats::Vector2 place);

    // The middle of the area of the button in the picture.
    Formats::Vector2 Place();

    // Gives back true when the balloon has begun to grow into view in this step.
    bool Calculate();

    void Draw();
    void DrawBalloon();

private:
    Display& display;
    std::unique_ptr<MenuLayout> layout;
    std::unique_ptr<FramePlayer> appearing;
    std::unique_ptr<FramePlayer> insert;
    std::unique_ptr<FramePlayer> rollOver;
    std::unique_ptr<FramePlayer> rollOut;
    std::unique_ptr<FramePlayer> press;
    std::unique_ptr<Balloon> balloon;
    bool isFocused = false;
};

}
