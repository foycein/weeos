#include "SystemMenu/WiiMenu/SdCardButton.h"

#include "SystemMenu/WiiMenu/LayoutParts.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view LayoutName = "mn_Sdcard_Btn";
constexpr std::string_view OutAnimation = "mn_Sdcard_Btn_BtnL_Out";
constexpr std::string_view InsertAnimation = "mn_Sdcard_Btn_Insert";
constexpr std::string_view RollOverAnimation = "mn_Sdcard_Btn_BtnL_RollOver";
constexpr std::string_view RollOutAnimation = "mn_Sdcard_Btn_BtnL_RollOut";
constexpr std::string_view PressAnimation = "mn_Sdcard_Btn_BtnL_On";

constexpr std::string_view InOutGroup = "Btn_L_InOut";
constexpr std::string_view InsertGroup = "Insert";
constexpr std::string_view RollGroup = "Btn_L_Roll";
constexpr std::string_view PressGroup = "Btn_L_On";

constexpr std::string_view AreaPane = "Ac";
constexpr std::string_view CardInsertedPane = "N_Btn_On";
constexpr std::string_view SlotEmptyPane = "N_Btn_Off";

// The button stands at the left of the bar: x -245 on a 16:9 screen and -152 on a 4:3 screen, y -172 (0x813EAA18).
constexpr Formats::Vector2 PlaceOnAWideScreen = {-245, -172};
constexpr Formats::Vector2 PlaceOnANarrowScreen = {-152, -172};

// The balloon appears 17 steps after the hand came, 50 units above the button, and keeps 200 units from the edges of a 16:9
// picture or 110 units from those of a 4:3 picture.
constexpr int StepsBeforeTheBalloonAppears = 17;
constexpr float BalloonHeightAboveTheButton = 50;
constexpr float BalloonMarginOnAWideScreen = 200;
constexpr float BalloonMarginOnANarrowScreen = 110;

}

SdCardButton::SdCardButton(Display& menuDisplay, const Layout::LayoutArchive& buttonArchive, const Layout::LayoutArchive& balloonArchive,
    const std::u16string& balloonText)
    : display(menuDisplay)
    , layout(std::make_unique<MenuLayout>(menuDisplay, buttonArchive, std::string(LayoutName)))
{
    appearing = std::make_unique<FramePlayer>(*layout, buttonArchive, OutAnimation, layout->Live().PanesOfGroup(InOutGroup));
    insert = std::make_unique<FramePlayer>(*layout, buttonArchive, InsertAnimation, layout->Live().PanesOfGroup(InsertGroup));
    rollOver = std::make_unique<FramePlayer>(*layout, buttonArchive, RollOverAnimation, layout->Live().PanesOfGroup(RollGroup));
    rollOut = std::make_unique<FramePlayer>(*layout, buttonArchive, RollOutAnimation, layout->Live().PanesOfGroup(RollGroup));
    press = std::make_unique<FramePlayer>(*layout, buttonArchive, PressAnimation, layout->Live().PanesOfGroup(PressGroup));

    // Out at its last frame: the button is not in view until it appears.
    appearing->Show(appearing->LastFrameOfTheFile());
    rollOut->Show(rollOut->LastFrameOfTheFile());
    ShowCardInserted(false);

    balloon = std::make_unique<Balloon>(menuDisplay, balloonArchive, StepsBeforeTheBalloonAppears);
    balloon->SetText(balloonText);

    bool isWide = display.GetScreenShape() == Platform::ScreenShape::SixteenByNine;
    MoveLayoutTo(*layout, isWide ? PlaceOnAWideScreen : PlaceOnANarrowScreen);
}

void SdCardButton::Appear()
{
    appearing->PlayBackwards(0, appearing->LastFrameOfTheFile());
}

void SdCardButton::ShowCardInserted(bool isInserted)
{
    layout->PaneNamed(CardInsertedPane).isVisible = isInserted;
    layout->PaneNamed(SlotEmptyPane).isVisible = !isInserted;
}

void SdCardButton::PlayInsert()
{
    insert->PlayOnce(0, insert->LastFrameOfTheFile());
}

void SdCardButton::SetFocused(bool shouldBeFocused)
{
    if (isFocused == shouldBeFocused)
        return;

    isFocused = shouldBeFocused;
    if (isFocused)
    {
        rollOver->PlayOnce(0, rollOver->LastFrameOfTheFile());
        balloon->PointerArrived();
        return;
    }

    rollOut->PlayOnce(0, rollOut->LastFrameOfTheFile());
    balloon->PointerLeft();
}

bool SdCardButton::IsFocused() const
{
    return isFocused;
}

const Balloon& SdCardButton::NameBalloon() const
{
    return *balloon;
}

void SdCardButton::PlayPress()
{
    press->PlayOnce(0, press->LastFrameOfTheFile());
}

bool SdCardButton::IsAt(Formats::Vector2 place)
{
    return PlacedPanes(*layout).IsOnPane(AreaPane, place);
}

Formats::Vector2 SdCardButton::Place()
{
    return PlacedPanes(*layout).PlaceOf(AreaPane).value_or(Formats::Vector2{0, 0});
}

bool SdCardButton::Calculate()
{
    appearing->Step();
    insert->Step();
    rollOver->Step();
    rollOut->Step();
    press->Step();
    layout->Calculate();

    bool isWide = display.GetScreenShape() == Platform::ScreenShape::SixteenByNine;
    Formats::Vector2 button = Place();
    balloon->PlaceAt({button.x, button.y + BalloonHeightAboveTheButton}, isWide ? BalloonMarginOnAWideScreen : BalloonMarginOnANarrowScreen);

    bool wasBalloonShown = balloon->IsShown();
    balloon->Calculate();
    return !wasBalloonShown && balloon->IsShown();
}

void SdCardButton::Draw()
{
    layout->Draw();
}

void SdCardButton::DrawBalloon()
{
    balloon->Draw();
}

}
