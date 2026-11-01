#include "SystemMenu/FrontPanel.h"

#include "SystemMenu/MenuStateFile.h"

namespace SystemMenu
{

FrontPanel::FrontPanel(Display& menuDisplay, Ios::PowerService& powerService, ScreenManager& screenManager, NandFiles& nandFiles)
    : power(powerService)
    , screens(screenManager)
    , nand(nandFiles)
    , fader(menuDisplay)
{
    fader.SetClear();
    fader.Calculate();
}

void FrontPanel::PressReset()
{
    Press(Ios::PowerEvent::ResetButtonPressed);
}

void FrontPanel::PressPower()
{
    Press(Ios::PowerEvent::PowerButtonPressed);
}

void FrontPanel::Press(Ios::PowerEvent button)
{
    if (phase != Phase::Idle)
        return;

    pressedButton = button;
    phase = Phase::WaitingForScreens;
}

bool FrontPanel::IsWorking() const
{
    return phase != Phase::Idle;
}

void FrontPanel::NoteMenuReached()
{
    isMenuReached = true;
}

void FrontPanel::Update(bool isErrorScreenShown)
{
    fader.Calculate();

    if (phase == Phase::Idle)
        TakeButtonOfThePlatform(isErrorScreenShown);

    if (phase == Phase::WaitingForScreens)
        WaitForScreens(isErrorScreenShown);
    else if (phase == Phase::FadingOut)
        WaitForTheBlack(isErrorScreenShown);
}

// The error screen listens to POWER only (0x81356C04; its RESET handler 0x81356C00 does nothing).
void FrontPanel::TakeButtonOfThePlatform(bool isErrorScreenShown)
{
    std::optional<Ios::PowerEvent> button = power.TryTakeEvent();
    if (!button)
        return;

    if (isErrorScreenShown && *button == Ios::PowerEvent::ResetButtonPressed)
        return;

    Press(*button);
}

void FrontPanel::WaitForScreens(bool isErrorScreenShown)
{
    if (!isErrorScreenShown && !screens.CanEveryScreenStop())
        return;

    if (!isErrorScreenShown)
        screens.TellEveryScreenToStop();

    fader.StartFadeOut();
    phase = Phase::FadingOut;
}

void FrontPanel::WaitForTheBlack(bool isErrorScreenShown)
{
    if (fader.GetStatus() != FaderStatus::Covered)
        return;

    if (!isErrorScreenShown && !screens.HasEveryScreenStopped())
        return;

    CarryOut();
}

void FrontPanel::CarryOut()
{
    phase = Phase::Done;

    if (pressedButton == Ios::PowerEvent::PowerButtonPressed)
    {
        RecordMenuLeft(nand, Formats::StateTypeShutDown);
        power.RequestShutdown();
        return;
    }

    if (!isMenuReached)
    {
        RecordMenuLeft(nand, Formats::StateTypeRestart);
        power.RequestReset();
        return;
    }

    RecordMenuLeft(nand, Formats::StateTypeReturnToMenu);
    titleToLaunch = Ios::SystemMenuTitleId;
}

void FrontPanel::Draw()
{
    fader.Draw();
}

std::optional<Ios::TitleId> FrontPanel::TakeTitleToLaunch()
{
    std::optional<Ios::TitleId> taken = titleToLaunch;
    titleToLaunch.reset();
    return taken;
}

}
