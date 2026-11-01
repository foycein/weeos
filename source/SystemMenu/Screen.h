#pragma once

#include "SystemMenu/ScreenId.h"
#include "SystemMenu/ScreenRequest.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace SystemMenu
{

// The manager draws the whole tree of screens three times in every picture, and a screen paints in the pass it wants.
// Nearly every screen paints in the Main pass. The Underlay pass is for what the Main pass needs to find in the picture first,
// the Overlay pass for what must lie over every other screen.
enum class DrawPass
{
    Underlay,
    Main,
    Overlay,
};

constexpr std::array<DrawPass, 3> AllDrawPasses = {DrawPass::Underlay, DrawPass::Main, DrawPass::Overlay};

// Where a screen is in its life. Every step the screen is in one of these states and does what the state says.
enum class ScreenState
{
    // Asks IsReady in every step until the screen can start.
    WaitingToStart,

    // Calls OnStart once, then runs the main part in the same step.
    Starting,

    // Calls RunMain in every step until the user is done with the screen.
    Running,

    // Calls OnRunEnded once, then runs the closing part in the same step.
    RunEnded,

    // Calls RunClosing in every step until the screen has said goodbye.
    Closing,

    // The manager removes the screen from the tree in this step.
    Finished,
};

// One screen of the menu, as the original builds it (docs/research/SystemMenuStructure.md 3.1 and 3.2).
// The screens form a tree. The manager makes a screen, calls Load, attaches it to the tree and calls Build.
// From then on it calls Step in every step and Draw in every pass.
class Screen
{
public:
    explicit Screen(ScreenId screenId);
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;
    virtual ~Screen() = default;

    ScreenId Id() const;
    ScreenState GetState() const;
    bool IsFinished() const;
    bool HasChildren() const;

    // Reads the files the screen needs. Called once, before the screen is attached to the tree.
    virtual void Load()
    {
    }

    // Makes the layouts and starts the first animations. Called once, when the screen has been attached to the tree.
    virtual void Build()
    {
    }

    virtual void Draw(DrawPass pass)
    {
        static_cast<void>(pass);
    }

    // Called once, when the manager takes the screen out of the tree, before the request that the screen left behind is posted.
    virtual void OnClose()
    {
    }

    // A screen that has screens below it stands still and is not drawn, as a dialog freezes the screen behind it.
    // A screen can ask to be updated or drawn all the same.
    virtual bool UpdatesWithChildren() const
    {
        return false;
    }

    virtual bool DrawsWithChildren() const
    {
        return false;
    }

    // The POWER or RESET button was pressed (the front panel, docs/audit/SystemMenu.md SYS-07). The menu waits until every screen
    // can stop, tells every screen to stop, and waits until every screen has stopped. Most screens can stop at any time.
    virtual bool CanStopForFrontPanel()
    {
        return true;
    }

    virtual void StopForFrontPanel()
    {
    }

    virtual bool HasStoppedForFrontPanel()
    {
        return true;
    }

    // One step of the life of the screen: moves it from state to state and calls what the state needs.
    void Step();

protected:
    virtual void BeforeStep()
    {
    }

    virtual void AfterStep()
    {
    }

    // Tells in WaitingToStart whether the screen can start.
    virtual bool IsReady()
    {
        return true;
    }

    virtual void OnStart()
    {
    }

    // Returns true when the user is done with the screen. The default screen runs for ever.
    virtual bool RunMain()
    {
        return false;
    }

    virtual void OnRunEnded()
    {
    }

    // Returns true when the screen has said goodbye and can be removed.
    virtual bool RunClosing()
    {
        return true;
    }

    // Asks for a new tree of screens to start with this screen. The request is posted when this screen is removed from the tree.
    void GoToRoot(ScreenId newFirstScreen, int argument);

private:
    friend class ScreenManager;

    void AdvanceState();

    ScreenId id;
    ScreenState state = ScreenState::WaitingToStart;
    Screen* parent = nullptr;
    std::vector<std::unique_ptr<Screen>> children;
    std::optional<ScreenRequest> requestWhenRemoved;
};

}
