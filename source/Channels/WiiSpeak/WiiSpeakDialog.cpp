#include "Channels/WiiSpeak/WiiSpeakDialog.h"

#include <array>
#include <string_view>

namespace Channels
{

namespace
{

constexpr std::string_view BundleName = "01030_dialog.arc";
constexpr std::string_view LayoutName = "01030_dialog";
constexpr std::string_view TextPane = "T_dialog_00";
constexpr std::string_view CirclePane = "N_circleRoot";
constexpr std::string_view CircleGroup = "G_circle_all";
const std::string CircleAnimation = "01030_dialog_circle_loop";
const std::string OpeningAnimation = "dialog_in";
const std::string ClosingAnimation = "dialog_out";

// The single button is the middle one; two buttons are the left and the right one.
constexpr int MiddleButton = 0;
constexpr int LeftButton = 1;
constexpr int RightButton = 2;
constexpr int ButtonPlaces = 3;

const std::array<std::string, ButtonPlaces> BoundPanes = {"dbutton_00", "dbutton_01", "dbutton_02"};
const std::array<std::string, ButtonPlaces> ButtonPanes = {"N_button_00", "N_button_01", "N_button_02"};
const std::array<std::string, ButtonPlaces> LabelPanes = {"T_button_00", "T_button_01", "T_button_02"};

// The sounds of the window, by their names in MicChatSound.brsar.
constexpr std::string_view AppearSound = "SE_WIN_APPEAR";
constexpr std::string_view PressSound = "SE_WIN_CLK_BTN_OK";

}

WiiSpeakDialog::WiiSpeakDialog(SystemMenu::Display& display, const WiiSpeakFiles& files)
    : layout(display, files.SystemLayouts(BundleName), std::string(LayoutName))
{
    for (const std::string& boundPane : BoundPanes)
        layout.AddButton(boundPane);

    std::size_t circleSlot = layout.Layout().BindAnimation(CircleAnimation, CircleGroup, Layout::PlaybackMode::Loop);
    layout.Layout().StartAnimation(circleSlot);
}

void WiiSpeakDialog::Show(const std::u16string& text, const std::vector<std::u16string>& buttonLabels)
{
    buttonCount = static_cast<int>(buttonLabels.size());
    layout.SetText(TextPane, text);
    layout.SetVisible(CirclePane, buttonCount == 0);

    ShowButton(MiddleButton, buttonCount == 1);
    ShowButton(LeftButton, buttonCount == 2);
    ShowButton(RightButton, buttonCount == 2);
    if (buttonCount == 1)
        layout.SetText(LabelPanes[MiddleButton], buttonLabels[0]);

    if (buttonCount == 2)
    {
        layout.SetText(LabelPanes[LeftButton], buttonLabels[0]);
        layout.SetText(LabelPanes[RightButton], buttonLabels[1]);
    }

    layout.PlayAnimation(OpeningAnimation);
    phase = Phase::Open;
    pendingChoice.reset();
    finishedChoice.reset();
}

void WiiSpeakDialog::Close()
{
    if (phase != Phase::Open)
        return;

    layout.PlayAnimation(ClosingAnimation);
    phase = Phase::Closing;
}

bool WiiSpeakDialog::IsOpen() const
{
    return phase != Phase::Closed;
}

std::optional<int> WiiSpeakDialog::TakeChoice()
{
    std::optional<int> choice = finishedChoice;
    finishedChoice.reset();
    return choice;
}

void WiiSpeakDialog::Update(const SystemMenu::InputManager& input, SystemMenu::SoundRequests& sounds)
{
    if (phase == Phase::Closed)
        return;

    std::optional<std::string> pressed = layout.TakePressedButton(input);
    if (phase == Phase::Open && pressed)
    {
        sounds.Request(PressSound);
        pendingChoice = FirstChoice;
        if (pressed == BoundPanes[RightButton])
            pendingChoice = SecondChoice;
        Close();
    }

    layout.Calculate();
    if (phase == Phase::Closing && !layout.IsAnimationPlaying(ClosingAnimation))
    {
        phase = Phase::Closed;
        finishedChoice = pendingChoice;
    }
}

void WiiSpeakDialog::Draw()
{
    if (phase != Phase::Closed)
        layout.Draw();
}

SceneLayout& WiiSpeakDialog::Layout()
{
    return layout;
}

void WiiSpeakDialog::ShowButton(int buttonNumber, bool isShown)
{
    layout.SetVisible(ButtonPanes[buttonNumber], isShown);
    layout.SetButtonEnabled(BoundPanes[buttonNumber], isShown);
}

}
