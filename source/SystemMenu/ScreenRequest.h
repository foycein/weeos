#pragma once

#include "SystemMenu/ScreenId.h"

#include <optional>

namespace SystemMenu
{

enum class RequestKind
{
    // Opens a screen below another screen.
    OpenChild,

    // Opens a screen in the place of a screen that is ending.
    ReplaceScreen,

    // Ends the whole tree of screens and starts a new tree with the screen as its first.
    GoToRoot,
};

// What a screen asks of the manager. Requests wait in a queue and are met one at a time, at the end of a step,
// so that no screen changes while a screen is running.
struct ScreenRequest
{
    RequestKind kind = RequestKind::OpenChild;
    ScreenId screen = ScreenId::Launcher;
    int argument = 0;

    // OpenChild and ReplaceScreen: below which screen the new one goes. Nothing means below the root.
    std::optional<ScreenId> parent;

    // OpenChild and ReplaceScreen: the new screen goes in front of this sibling. Nothing means behind all the others.
    std::optional<ScreenId> nextSibling;

    // ReplaceScreen: the screen that is replaced. The new screen takes its place as soon as that screen is gone.
    std::optional<ScreenId> replacedScreen;
};

}
