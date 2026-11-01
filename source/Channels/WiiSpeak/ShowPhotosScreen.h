#pragma once

#include "Channels/Photo/PhotoLibrary.h"
#include "Channels/Photo/ShownPicture.h"
#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace Channels
{

// Show Photos from the SD card (the photo viewer of layout.arc.LZ, built like the Photo Channel's): the photos of the card one at a
// time with Previous and Next, Show This Photo, which prints it in the chat room, and Finish. Without a card, with a card that cannot
// be read or without photos the channel says so and goes back to the chat room. The card is searched as the Photo Channel searches it.
// The thumbnails and the Wii Message Board as a place of photos are not built.
class ShowPhotosScreen : public WiiSpeakScreen
{
public:
    explicit ShowPhotosScreen(WiiSpeakSession& session);

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

    std::size_t PhotoCount() const;
    std::size_t ShownPhotoNumber() const;

private:
    enum class Waiting
    {
        Nothing,
        LeavingAfterTheMessage,
        AnswerToFinishing,
    };

    void FindThePhotos();
    void LeaveWithMessage(std::uint32_t messageId);
    void ShowPhoto(std::size_t number);
    std::optional<WiiSpeakScreenName> Press(const std::string& button);

    WiiSpeakSession& session;
    SceneLayout layout;
    std::vector<PhotoFile> photos;
    std::size_t shownPhoto = 0;
    std::optional<Formats::RgbaImage> photo;
    std::unique_ptr<ShownPicture> picture;
    Waiting waiting = Waiting::Nothing;
};

}
