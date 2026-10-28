#include "SystemMenu/WiiOptionsBarScreen.h"

#include "SystemMenu/Fader.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/SoundRequests.h"

#include "Text/MessageCatalog.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "setupBtn";
constexpr std::string_view LayoutName = "it_Button_a";

constexpr std::string_view BackButtonPane = "B_Button_00";
constexpr std::string_view BackButtonTextPane = "T_Button_00";

// The text of the button is the message "Back".
constexpr std::uint16_t BackMessage = 315;

// The groups of the layout that the animations play on.
constexpr std::string_view BarGroup = "G_BarIn";
constexpr std::string_view FocusedButtonGroup = "G_FocusBtnA";
constexpr std::string_view SelectedButtonGroup = "G_SelectBtnA";
constexpr std::string_view IconGroup = "G_Wii";

}

WiiOptionsBarScreen::WiiOptionsBarScreen(MenuServices& menuServices)
    : Screen(ScreenId::WiiOptionsBar)
    , services(menuServices)
{
}

void WiiOptionsBarScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void WiiOptionsBarScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));
    layout->SetText(BackButtonTextPane, services.messages.Find(BackMessage));

    slideInSlot = layout->BindAnimation("it_Button_a_SeenIn", BarGroup, Layout::PlaybackMode::PlayOnce);
    focusInSlot = layout->BindAnimation("it_Button_a_BtnFoucusIn", FocusedButtonGroup, Layout::PlaybackMode::PlayOnce);
    focusOutSlot = layout->BindAnimation("it_Button_a_BtnFoucusOut", FocusedButtonGroup, Layout::PlaybackMode::PlayOnce);
    flashSlot = layout->BindAnimation("it_Button_a_BtnFlash", SelectedButtonGroup, Layout::PlaybackMode::PlayOnce);
    iconAppearSlot = layout->BindAnimation("it_Button_a_WiiAppear", IconGroup, Layout::PlaybackMode::PlayOnce);
    iconLostSlot = layout->BindAnimation("it_Button_a_WiiLost", IconGroup, Layout::PlaybackMode::PlayOnce);
    layout->Calculate();

    buttons = std::make_unique<ButtonController>(*layout, services.display);
    buttons->AddButton(std::string(BackButtonPane));
    buttons->SetEnabled(BackButtonPane, true);
}

void WiiOptionsBarScreen::Draw(DrawPass pass)
{
    if (pass == DrawPass::Main)
        layout->Draw();
}

// The bar slides in as soon as the picture has faded in. The screen is ready when the bar is in its place.
bool WiiOptionsBarScreen::IsReady()
{
    if (!hasStartedSlidingIn)
    {
        if (services.fader.GetStatus() == FaderStatus::Clear)
        {
            hasStartedSlidingIn = true;
            layout->StartAnimation(slideInSlot);
        }
        return false;
    }

    return !layout->IsAnimationPlaying(slideInSlot);
}

void WiiOptionsBarScreen::AfterStep()
{
    layout->Calculate();
}

bool WiiOptionsBarScreen::WasBackButtonPressed()
{
    if (GetState() != ScreenState::Running)
        return false;

    if (services.fader.GetStatus() == FaderStatus::Clear)
        HandleButtonEvents(buttons->Poll(services.input));

    bool wasPressed = isBackButtonPressed;
    isBackButtonPressed = false;
    return wasPressed;
}

void WiiOptionsBarScreen::HandleButtonEvents(const std::vector<ButtonEvent>& events)
{
    for (const ButtonEvent& event : events)
    {
        if (event.kind == ButtonEventKind::FocusGained)
        {
            services.sounds.Request(SoundName::ButtonTargeted);
            layout->StartAnimation(focusInSlot);
        }
        else if (event.kind == ButtonEventKind::FocusLost)
        {
            layout->StartAnimation(focusOutSlot);
        }
        else
        {
            services.sounds.Request(SoundName::Cancel);
            layout->StartAnimation(flashSlot);
            isBackButtonPressed = true;
            buttons->ForgetPointers();
        }
    }
}

bool WiiOptionsBarScreen::IsBackButtonFlashing() const
{
    return layout->IsAnimationPlaying(flashSlot);
}

void WiiOptionsBarScreen::ShowWiiIcon()
{
    layout->StartAnimation(iconAppearSlot);
}

void WiiOptionsBarScreen::HideWiiIcon()
{
    layout->StartAnimation(iconLostSlot);
}

}
