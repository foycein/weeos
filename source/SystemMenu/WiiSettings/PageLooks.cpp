#include "SystemMenu/WiiSettings/PageLooks.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view ButtonFolder = "FIX/COMMON/BTN/";
constexpr std::string_view BackgroundFolder = "FIX/COMMON/BG/";

std::string ButtonPicture(std::string_view fileName)
{
    return std::string(ButtonFolder) + std::string(fileName);
}

std::string BackgroundPicture(std::string_view fileName)
{
    return std::string(BackgroundFolder) + std::string(fileName);
}

LookPictures MakeMenuItemLook()
{
    LookPictures look;
    look.background = ButtonPicture("Btn_List03.gif");
    look.idleOverlay = ButtonPicture("List03_off.png");
    look.pointedOverlay = ButtonPicture("List03_on.png");
    return look;
}

// A choice has a frame around it when it is chosen. The frame is 13 points wider than the button on each side and above and below it.
LookPictures MakeChoiceItemLook()
{
    LookPictures look;
    look.background = ButtonPicture("Btn_List.gif");
    look.idleOverlay = ButtonPicture("List_off.png");
    look.pointedOverlay = ButtonPicture("List_on.png");
    look.hasChosenFrame = true;
    look.chosenFrameLeft = {BackgroundPicture("List_flame_L.gif"), -13, -13};
    look.chosenFrameRight = {BackgroundPicture("List_flame_R.gif"), -11, -13};
    return look;
}

LookPictures MakeBottomLook()
{
    LookPictures look;
    look.background = ButtonPicture("Under.gif");
    look.idleOverlay = ButtonPicture("Under_off.png");
    look.pointedOverlay = ButtonPicture("Under_on.png");
    return look;
}

LookPictures MakeListRowLook()
{
    LookPictures look;
    look.background = ButtonPicture("Btn_List06.gif");
    look.idleOverlay = ButtonPicture("List06_off.png");
    look.pointedOverlay = ButtonPicture("List06_on.png");
    look.hasChosenFrame = true;
    look.chosenFrameLeft = {BackgroundPicture("List_flame_M_L.gif"), -9, -4};
    look.chosenFrameRight = {BackgroundPicture("List_flame_M_R.gif"), -17, -4};
    return look;
}

LookPictures MakeArrowLook(std::string_view direction)
{
    LookPictures look;
    look.idleOverlay = ButtonPicture("Allow_" + std::string(direction) + "_off.gif");
    look.pointedOverlay = ButtonPicture("Allow_" + std::string(direction) + "_on.gif");
    return look;
}

LookPictures MakeCircleLook(std::string_view fileName)
{
    LookPictures look;
    look.idleOverlay = BackgroundPicture(fileName);
    look.pointedOverlay = BackgroundPicture(fileName);
    return look;
}

}

const LookPictures& PicturesOfLook(ButtonLook look)
{
    static const LookPictures menuItem = MakeMenuItemLook();
    static const LookPictures choiceItem = MakeChoiceItemLook();
    static const LookPictures bottom = MakeBottomLook();
    static const LookPictures listRow = MakeListRowLook();
    static const LookPictures arrowUp = MakeArrowLook("U");
    static const LookPictures arrowDown = MakeArrowLook("D");
    static const LookPictures minusCircle = MakeCircleLook("DPD_Minus_btn.gif");
    static const LookPictures plusCircle = MakeCircleLook("DPD_Plus_btn.gif");

    switch (look)
    {
    case ButtonLook::MenuItem:
        return menuItem;

    case ButtonLook::ChoiceItem:
        return choiceItem;

    case ButtonLook::BottomLeft:
    case ButtonLook::BottomRight:
        return bottom;

    case ButtonLook::ListRow:
        return listRow;

    case ButtonLook::ArrowUp:
        return arrowUp;

    case ButtonLook::ArrowDown:
        return arrowDown;

    case ButtonLook::MinusCircle:
        return minusCircle;

    case ButtonLook::PlusCircle:
        return plusCircle;
    }

    return menuItem;
}

}
