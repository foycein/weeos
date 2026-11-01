#pragma once

#include "Channels/Photo/ShownPicture.h"
#include "Channels/WiiSpeak/ChatPhotos.h"
#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace Channels
{

// Show Photos (the photo viewer of layout.arc.LZ, built like the Photo Channel's):
// - the place of the photos ("title" with "title_sd0" and "title_wii0"): the SD card or the Wii Message Board, and Back;
// - the thumbnails ("thumb"): rows of small photos scrolled with Up and Down, larger and smaller with the zoom, Change Photo
//   Location and Finish;
// - one photo ("view"): Previous, Next, Thumbnails, Show This Photo, which prints it in the chat room, and Finish.
// A missing or unreadable card and a place without photos are told in the message window.
class ShowPhotosScreen : public WiiSpeakScreen
{
public:
    enum class Stage
    {
        ChoosingThePlace,
        Thumbnails,
        Viewing,
    };

    explicit ShowPhotosScreen(WiiSpeakSession& session);

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

    Stage CurrentStage() const;
    std::size_t PhotoCount() const;
    std::size_t ShownPhotoNumber() const;
    std::size_t ColumnsOfThumbnails() const;

    // Where the thumbnail of a photo is drawn while the list shows it.
    PictureBox BoxOfPhoto(std::size_t number) const;

private:
    enum class Waiting
    {
        Nothing,
        Message,
        AnswerToFinishing,
    };

    std::optional<WiiSpeakScreenName> UpdatePlace(const std::string& pressed);
    void UpdateThumbnails(const std::optional<std::string>& pressed);
    void ViewPhotoAtPress();
    std::size_t NeighborPhoto(bool isNext) const;
    std::optional<WiiSpeakScreenName> UpdateView(const std::string& pressed);
    void OpenThePhotos(std::vector<ChatPhoto> foundPhotos, std::uint32_t noPhotosMessageId);
    void OpenCardPhotos();
    void TellAndStay(std::uint32_t messageId);
    void AskToFinish();

    void ShowPhoto(std::size_t number);
    void MakeSomeThumbnails();
    void DrawThumbnails();
    std::optional<std::size_t> PhotoAtPress() const;
    float CellWidth() const;
    float CellHeight() const;
    std::size_t VisibleRows() const;
    std::size_t RowCount() const;
    std::size_t FirstShownPhoto() const;
    std::size_t EndOfShownPhotos() const;
    void Scroll(int rows);
    void ChangeZoom(int step);

    WiiSpeakSession& session;
    Stage stage = Stage::ChoosingThePlace;
    Waiting waiting = Waiting::Nothing;

    SceneLayout placeTitle;
    SceneLayout cardButton;
    SceneLayout boardButton;
    SceneLayout thumbnailButtons;
    SceneLayout viewButtons;

    std::vector<ChatPhoto> photos;
    std::map<std::size_t, std::unique_ptr<ShownPicture>> thumbnails;
    std::set<std::size_t> unreadablePhotos;
    std::unique_ptr<ShownPicture> unreadableMarker;
    std::size_t firstRow = 0;
    std::size_t zoom;

    std::size_t shownPhoto = 0;
    std::optional<Formats::RgbaImage> photo;
    std::unique_ptr<ShownPicture> picture;
};

}
