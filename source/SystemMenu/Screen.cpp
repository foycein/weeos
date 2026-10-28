#include "SystemMenu/Screen.h"

namespace SystemMenu
{

Screen::Screen(ScreenId screenId)
    : id(screenId)
{
}

ScreenId Screen::Id() const
{
    return id;
}

ScreenState Screen::GetState() const
{
    return state;
}

bool Screen::IsFinished() const
{
    return state == ScreenState::Finished;
}

bool Screen::HasChildren() const
{
    return !children.empty();
}

void Screen::Step()
{
    if (IsFinished())
        return;

    BeforeStep();
    AdvanceState();

    if (!IsFinished())
        AfterStep();
}

void Screen::AdvanceState()
{
    switch (state)
    {
    case ScreenState::WaitingToStart:
        if (IsReady())
            state = ScreenState::Starting;
        return;

    case ScreenState::Starting:
        OnStart();
        state = ScreenState::Running;
        [[fallthrough]];

    case ScreenState::Running:
        if (RunMain())
            state = ScreenState::RunEnded;
        return;

    case ScreenState::RunEnded:
        OnRunEnded();
        state = ScreenState::Closing;
        [[fallthrough]];

    case ScreenState::Closing:
        if (RunClosing())
            state = ScreenState::Finished;
        return;

    case ScreenState::Finished:
        return;
    }
}

void Screen::GoToRoot(ScreenId newFirstScreen, int argument)
{
    ScreenRequest request;
    request.kind = RequestKind::GoToRoot;
    request.screen = newFirstScreen;
    request.argument = argument;
    requestWhenRemoved = request;
}

}
