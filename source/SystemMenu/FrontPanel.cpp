#include "SystemMenu/FrontPanel.h"

namespace SystemMenu
{

FrontPanel::FrontPanel(Display& menuDisplay, Ios::PowerService& powerService)
    : power(powerService)
    , fader(menuDisplay)
{
    fader.SetClear();
    fader.Calculate();
}

void FrontPanel::Update()
{
    if (!pressedButton)
    {
        std::optional<Ios::PowerEvent> button = power.TryTakeEvent();
        if (button)
            StartFading(*button);
    }

    fader.Calculate();

    if (pressedButton && !hasAsked && fader.GetStatus() == FaderStatus::Covered)
        AskForWhatTheButtonMeans();
}

void FrontPanel::StartFading(Ios::PowerEvent button)
{
    pressedButton = button;
    fader.StartFadeOut();
}

void FrontPanel::AskForWhatTheButtonMeans()
{
    hasAsked = true;

    if (*pressedButton == Ios::PowerEvent::ResetButtonPressed)
        power.RequestReset();
    else
        power.RequestShutdown();
}

void FrontPanel::Draw()
{
    fader.Draw();
}

}
