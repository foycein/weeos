#include "SystemMenu/ScreenManager.h"

#include "SystemMenu/LauncherScreen.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace SystemMenu
{

namespace
{

bool CanBeUpdated(const Screen& screen)
{
    bool isFrozenByChildren = screen.HasChildren() && !screen.UpdatesWithChildren();
    return !isFrozenByChildren && !screen.IsFinished();
}

bool CanBeDrawn(const Screen& screen)
{
    bool isHiddenByChildren = screen.HasChildren() && !screen.DrawsWithChildren();
    return !isHiddenByChildren && !screen.IsFinished();
}

}

ScreenManager::ScreenManager(ScreenFactory screenFactory)
    : makeScreen(std::move(screenFactory))
{
}

void ScreenManager::Start(ScreenId firstScreen, int argument)
{
    root = std::make_unique<LauncherScreen>(*this, firstScreen, argument);
    root->Build();
}

void ScreenManager::OpenChild(ScreenId screen, ScreenId parent, int argument)
{
    ScreenRequest request;
    request.kind = RequestKind::OpenChild;
    request.screen = screen;
    request.argument = argument;
    request.parent = parent;
    Post(request);
}

void ScreenManager::ReplaceScreen(const Screen& replacedScreen, ScreenId screen, int argument)
{
    ScreenRequest request;
    request.kind = RequestKind::ReplaceScreen;
    request.screen = screen;
    request.argument = argument;
    request.replacedScreen = replacedScreen.Id();

    const Screen* parent = replacedScreen.parent;
    if (parent != nullptr)
    {
        request.parent = parent->Id();

        auto position = std::find_if(parent->children.begin(), parent->children.end(),
            [&](const std::unique_ptr<Screen>& child) { return child.get() == &replacedScreen; });
        if (position != parent->children.end() && position + 1 != parent->children.end())
            request.nextSibling = (*(position + 1))->Id();
    }
    Post(request);
}

void ScreenManager::Post(const ScreenRequest& request)
{
    if (pendingRequests.size() >= MaximumPendingRequests)
        return;

    pendingRequests.push_back(request);
}

void ScreenManager::Update()
{
    if (root)
        UpdateScreens(*root);

    if (!loadingScreen)
        MeetNextRequest();
    else if (isLoadingScreenArmed)
        FinishLoading();

    if (root)
        RemoveFinishedScreens(*root);
}

void ScreenManager::Draw()
{
    if (!root)
        return;

    for (DrawPass pass : AllDrawPasses)
        DrawScreens(*root, pass);
}

Screen* ScreenManager::FindScreen(ScreenId screen)
{
    if (!root)
        return nullptr;

    std::vector<Screen*> toVisit = {root.get()};
    while (!toVisit.empty())
    {
        Screen* visited = toVisit.back();
        toVisit.pop_back();

        if (visited->Id() == screen)
            return visited;

        for (const std::unique_ptr<Screen>& child : visited->children)
            toVisit.push_back(child.get());
    }
    return nullptr;
}

std::optional<ScreenId> ScreenManager::UnbuiltScreen() const
{
    return unbuiltScreen;
}

void ScreenManager::UpdateScreens(Screen& screen)
{
    if (CanBeUpdated(screen))
        screen.Step();

    for (const std::unique_ptr<Screen>& child : screen.children)
        UpdateScreens(*child);
}

void ScreenManager::DrawScreens(Screen& screen, DrawPass pass)
{
    if (CanBeDrawn(screen))
        screen.Draw(pass);

    for (const std::unique_ptr<Screen>& child : screen.children)
        DrawScreens(*child, pass);
}

void ScreenManager::MeetNextRequest()
{
    if (pendingRequests.empty())
        return;

    ScreenRequest request = pendingRequests.front();
    pendingRequests.pop_front();

    if (request.kind == RequestKind::GoToRoot)
        GoToNewRoot(request);
    else
        BeginLoading(request);
}

void ScreenManager::BeginLoading(const ScreenRequest& request)
{
    std::unique_ptr<Screen> screen = makeScreen(request.screen, request.argument);
    if (!screen)
    {
        unbuiltScreen = request.screen;
        return;
    }

    screen->Load();
    loadingScreen = std::move(screen);
    loadingRequest = request;
    isLoadingScreenArmed = request.kind == RequestKind::OpenChild;
}

void ScreenManager::FinishLoading()
{
    Screen* parent = FindParentFor(loadingRequest);
    std::unique_ptr<Screen> screen = std::move(loadingScreen);
    isLoadingScreenArmed = false;

    // The screen that the new one should go below can be gone by now. Then the new screen has nowhere to go.
    if (parent == nullptr)
        return;

    Screen* attachedScreen = screen.get();
    Attach(*parent, std::move(screen), loadingRequest.nextSibling);
    attachedScreen->Build();
}

void ScreenManager::GoToNewRoot(const ScreenRequest& request)
{
    if (root)
        RemoveScreen(*root);

    Start(request.screen, request.argument);
}

Screen* ScreenManager::FindParentFor(const ScreenRequest& request)
{
    if (!request.parent)
        return root.get();

    return FindScreen(*request.parent);
}

void ScreenManager::Attach(Screen& parent, std::unique_ptr<Screen> screen, std::optional<ScreenId> nextSibling)
{
    auto position = parent.children.end();
    if (nextSibling)
    {
        position = std::find_if(parent.children.begin(), parent.children.end(),
            [&](const std::unique_ptr<Screen>& child) { return child->Id() == *nextSibling; });
    }

    screen->parent = &parent;
    parent.children.insert(position, std::move(screen));
}

void ScreenManager::RemoveFinishedScreens(Screen& screen)
{
    std::vector<Screen*> finishedChildren;
    for (const std::unique_ptr<Screen>& child : screen.children)
    {
        if (child->IsFinished())
            finishedChildren.push_back(child.get());
    }

    for (Screen* finished : finishedChildren)
        RemoveScreen(*finished);

    for (const std::unique_ptr<Screen>& child : screen.children)
        RemoveFinishedScreens(*child);
}

// The screens below go first, then the screen itself. A request that a screen left behind is posted as it goes.
void ScreenManager::RemoveScreen(Screen& screen)
{
    while (screen.HasChildren())
        RemoveScreen(*screen.children.front());

    if (loadingScreen && loadingRequest.replacedScreen == screen.Id())
        isLoadingScreenArmed = true;

    screen.OnClose();
    if (screen.requestWhenRemoved)
        Post(*screen.requestWhenRemoved);

    if (screen.parent == nullptr)
    {
        root.reset();
        return;
    }

    std::erase_if(screen.parent->children, [&](const std::unique_ptr<Screen>& child) { return child.get() == &screen; });
}

}
