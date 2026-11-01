#include "Channels/Photo/PhotoSession.h"

// What the channel shows at its start when its save file is not right: "Unable to create a save file." with "Back to the Wii Menu"
// (layout error1, message 1), or "The save file is corrupted. Now formatting Photo Channel save data." with OK (layout error2), after
// which the new save file is there and the title screen follows.

namespace Channels
{

namespace
{

constexpr std::string_view CannotCreateMessage = "textTUS1";
constexpr std::string_view SystemMemoryMessage = "textTUS0";

class SaveErrorScreen : public PhotoScreen
{
public:
    SaveErrorScreen(PhotoSession& photoSession, PhotoStorage::StartResult startResult)
        : session(photoSession)
        , result(startResult)
        , message(session.MakeLayout(startResult == PhotoStorage::StartResult::CannotCreate ? "error1" : "error2"))
    {
        if (result != PhotoStorage::StartResult::CannotCreate)
            return;

        message->PaneNamed(SystemMemoryMessage).isVisible = false;
        message->PaneNamed(CannotCreateMessage).isVisible = true;
    }

    void Update() override
    {
        message->Calculate();
        if (result == PhotoStorage::StartResult::CannotCreate && session.IsButtonPressed(*message, "no"))
        {
            session.Services().sounds.Request(PhotoSound::Cancel);
            session.Services().wiiMenu.Request();
        }
        else if (result == PhotoStorage::StartResult::Corrupted && session.IsButtonPressed(*message, "next"))
        {
            session.Services().sounds.Request(PhotoSound::Decide);
            session.GoTo(MakeTitleScreen(session));
        }
    }

    void Draw() override
    {
        message->Draw();
    }

    std::string_view Name() const override
    {
        return result == PhotoStorage::StartResult::CannotCreate ? "Unable to create a save file" : "Save file corrupted";
    }

private:
    PhotoSession& session;
    PhotoStorage::StartResult result;
    std::unique_ptr<SystemMenu::MenuLayout> message;
};

}

std::unique_ptr<PhotoScreen> MakeSaveErrorScreen(PhotoSession& session, PhotoStorage::StartResult result)
{
    return std::make_unique<SaveErrorScreen>(session, result);
}

}
