#pragma once

#include "SystemMenu/Screen.h"
#include "SystemMenu/ScreenRequest.h"

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace SystemMenu
{

// Makes the screen with this id. Gives back nothing when that screen is not built yet.
using ScreenFactory = std::function<std::unique_ptr<Screen>(ScreenId screen, int argument)>;

// The tree of screens and the requests that change it.
//
// The root of the tree is a launcher screen that opens the first screen. Everything else is a child or a grandchild.
// In every step the manager updates the screens, meets one request, and removes the screens that have finished.
// A new screen is made and loaded first, and attached to the tree in the next step; a screen that replaces another one
// waits until that one is gone.
class ScreenManager
{
public:
    explicit ScreenManager(ScreenFactory screenFactory);

    // Starts the menu: forgets what was going on and makes the launcher, which opens the first screen.
    void Start(ScreenId firstScreen, int argument);

    // Requests. A request is dropped when eight others wait already.
    void OpenChild(ScreenId screen, ScreenId parent, int argument);

    // The same, with the new screen in front of a sibling, so that it is drawn before it.
    void OpenChildBefore(ScreenId screen, ScreenId parent, ScreenId nextSibling, int argument);
    void ReplaceScreen(const Screen& replacedScreen, ScreenId screen, int argument);

    void Update();

    // Draws the whole tree in the three passes.
    void Draw();

    // The screen with this id, or nothing when it is not in the tree.
    Screen* FindScreen(ScreenId screen);

    // The screen that was asked for last and that the factory could not make, so that the program can tell the user.
    std::optional<ScreenId> UnbuiltScreen() const;

    // The front panel asks every screen in the tree before a reset or a power off (Screen::CanStopForFrontPanel).
    bool CanEveryScreenStop();
    void TellEveryScreenToStop();
    bool HasEveryScreenStopped();

    // True while a screen that was asked for has been made and loaded, and is not in the tree yet.
    bool IsLoadingScreen() const;

private:
    // Every screen in the tree, a screen before the screens below it.
    std::vector<Screen*> AllScreens();

    static constexpr std::size_t MaximumPendingRequests = 8;

    void Post(const ScreenRequest& request);

    void UpdateScreens(Screen& screen);
    void DrawScreens(Screen& screen, DrawPass pass);

    void MeetNextRequest();
    void BeginLoading(const ScreenRequest& request);
    void FinishLoading();
    void GoToNewRoot(const ScreenRequest& request);
    void MakeLauncher(ScreenId firstScreen, int argument);

    Screen* FindParentFor(const ScreenRequest& request);
    void Attach(Screen& parent, std::unique_ptr<Screen> screen, std::optional<ScreenId> nextSibling);

    void RemoveFinishedScreens(Screen& screen);
    void RemoveScreen(Screen& screen);

    ScreenFactory makeScreen;
    std::unique_ptr<Screen> root;
    std::deque<ScreenRequest> pendingRequests;

    // The screen that has been made and loaded and waits to be attached to the tree, and the request that made it.
    std::unique_ptr<Screen> loadingScreen;
    ScreenRequest loadingRequest;

    // True when the loading screen may be attached. A replacing screen is not armed before the screen it replaces is gone.
    bool isLoadingScreenArmed = false;

    std::optional<ScreenId> unbuiltScreen;
};

}
