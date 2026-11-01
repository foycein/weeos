#include "Channels/Photo/PhotoPictures.h"
#include "Channels/Photo/PhotoSession.h"
#include "Channels/Photo/ShownPicture.h"

#include "SystemMenu/MessageBoard/BoardPosting.h"

#include "Media/JpegEncoder.h"
#include "Media/PictureShapes.h"

#include <chrono>
#include <random>

#include <algorithm>
#include <cmath>

// Post (layout "today_select"): the photo to the Wii Menu (layouts menu, menu_what, menu_save, menu_comp, menu_ng), and the removal of
// the Wii Menu photo (menu_del, menu_del_save, menu_del_comp, menu_del_ng). The photo is shown in the channel's icon and preview:
// noerase/photo_i.dat (160 x 120) and photo_b.dat (480 x 272). + and - make the part of the photo smaller and larger, the
// +Control Pad moves it (the tip of the tips window: "you can adjust the photo by pressing the + or - Buttons").
// Post to the Wii Message Board (layouts today, today_what, today_save, today_comp, today_ng) puts the photo, with its changes, as a
// JPEG picture on a letter of this console on today's page of the board.

namespace Channels
{

namespace
{

// The board keeps a photo at most 640 x 480, written as a JPEG picture of this quality (Assumed, CHN-33).
constexpr int BoardPictureLargestWidth = 640;
constexpr int BoardPictureLargestHeight = 480;
constexpr int BoardPictureQuality = 90;
constexpr std::string_view PhotoQuestion = "textTUS0A";
constexpr std::string_view MovieFrameQuestion = "textTUS1A";
constexpr std::string_view NewPhotoQuestion = "textTUS2A";
constexpr std::string_view SaveSound = "SE_SAVE";

// How the part of the photo can be changed (Assumed, CHN-33): the zoom grows by a quarter per press, up to four times; a press of the
// +Control Pad moves the part by a twentieth of the photo.
constexpr float ZoomStep = 1.25f;
constexpr float LargestZoom = 4;
constexpr float MoveStep = 0.05f;

// "Posting to the Wii Menu..." shows this long before the files are written (Assumed, CHN-33).
constexpr int StepsOfSaving = 60;

// The part of the photo with the shape of the target, the given zoom and its middle at a share of the photo, scaled to the target.
Formats::RgbaImage CropForPost(const Formats::RgbaImage& photo, int targetWidth, int targetHeight, Formats::Vector2 middle, float zoom)
{
    float targetAspect = static_cast<float>(targetWidth) / static_cast<float>(targetHeight);
    float partWidth = static_cast<float>(photo.width);
    float partHeight = partWidth / targetAspect;
    if (partHeight > photo.height)
    {
        partHeight = static_cast<float>(photo.height);
        partWidth = partHeight * targetAspect;
    }
    partWidth /= zoom;
    partHeight /= zoom;

    float left = std::clamp(middle.x * photo.width - partWidth / 2, 0.0f, photo.width - partWidth);
    float top = std::clamp(middle.y * photo.height - partHeight / 2, 0.0f, photo.height - partHeight);

    Formats::RgbaImage crop{static_cast<std::uint32_t>(targetWidth), static_cast<std::uint32_t>(targetHeight), {}};
    crop.pixels.reserve(static_cast<std::size_t>(targetWidth) * targetHeight);
    for (int y = 0; y < targetHeight; ++y)
    {
        for (int x = 0; x < targetWidth; ++x)
        {
            int sourceX = std::min(static_cast<int>(left + (x + 0.5f) * partWidth / targetWidth), static_cast<int>(photo.width) - 1);
            int sourceY = std::min(static_cast<int>(top + (y + 0.5f) * partHeight / targetHeight), static_cast<int>(photo.height) - 1);
            crop.pixels.push_back(photo.pixels[static_cast<std::size_t>(sourceY) * photo.width + sourceX]);
        }
    }
    return crop;
}

class PostScreen : public PhotoScreen
{
public:
    enum class Stage
    {
        Choosing,
        Adjusting,
        AboutPosting,
        Saving,
        Posted,
        PostFailed,
        AskingToPostToBoard,
        AboutBoard,
        SavingToBoard,
        PostedToBoard,
        BoardFailed,
        AskingToRemove,
        Removing,
        Removed,
        RemoveFailed,
    };

    PostScreen(PhotoSession& photoSession, int index)
        : session(photoSession)
        , fileIndex(index)
        , layout(session.MakeLayout("today_select"))
    {
        photo = session.PhotoOfFile(fileIndex).value_or(Formats::RgbaImage{1, 1, {{0, 0, 0, 255}}});
    }

    void Update() override
    {
        layout->Calculate();
        ++stepsOfStage;
        switch (stage)
        {
        case Stage::Choosing:
            UpdateChoosing();
            break;
        case Stage::Adjusting:
            UpdateAdjusting();
            break;
        case Stage::AboutPosting:
            UpdateAboutPosting();
            break;
        case Stage::Saving:
            UpdateSaving();
            break;
        case Stage::Posted:
            BackToView("next");
            break;
        case Stage::PostFailed:
        case Stage::Removed:
        case Stage::RemoveFailed:
            BackToView("back");
            break;
        case Stage::AskingToRemove:
            UpdateAskingToRemove();
            break;
        case Stage::AskingToPostToBoard:
            UpdateAskingToPostToBoard();
            break;
        case Stage::AboutBoard:
            if (session.IsButtonPressed(*layout, "back"))
                StartAskingToPostToBoard();
            break;
        case Stage::SavingToBoard:
            UpdateSavingToBoard();
            break;
        case Stage::PostedToBoard:
            BackToView("next");
            break;
        case Stage::BoardFailed:
            BackToView("back");
            break;
        case Stage::Removing:
            UpdateRemoving();
            break;
        }
    }

    void Draw() override
    {
        layout->Draw();
        if (preview && layout->FindPane("picture") != nullptr)
            preview->DrawInside(session.BoxOfPane(*layout, "picture"));
    }

    std::string_view Name() const override
    {
        switch (stage)
        {
        case Stage::Choosing:
            return "Post";
        case Stage::Adjusting:
            return "Post to the Wii Menu?";
        case Stage::AboutPosting:
            return "About Posting to the Wii Menu";
        case Stage::Saving:
            return "Posting to the Wii Menu";
        case Stage::Posted:
            return "Posted to the Wii Menu";
        case Stage::PostFailed:
            return "Not posted";
        case Stage::AskingToRemove:
            return "Remove Wii Menu Photo?";
        case Stage::Removing:
            return "Removing the Wii Menu Photo";
        case Stage::Removed:
            return "Removed from the Wii Menu";
        case Stage::RemoveFailed:
            return "Not removed";
        case Stage::AskingToPostToBoard:
            return "Post to the Wii Message Board?";
        case Stage::AboutBoard:
            return "About Posting to the Wii Message Board";
        case Stage::SavingToBoard:
            return "Posting to the Wii Message Board";
        case Stage::PostedToBoard:
            return "Posted to the Wii Message Board";
        case Stage::BoardFailed:
            return "Not posted to the Wii Message Board";
        }
        return "Post";
    }

private:
    void Show(Stage newStage, std::string_view layoutName)
    {
        stage = newStage;
        stepsOfStage = 0;
        layout = session.MakeLayout(layoutName);
    }

    void UpdateChoosing()
    {
        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*layout, "board"))
        {
            services.sounds.Request(PhotoSound::Decide);
            StartAskingToPostToBoard();
        }
        else if (session.IsButtonPressed(*layout, "menu"))
        {
            services.sounds.Request(PhotoSound::Decide);
            StartAdjusting();
        }
        else if (session.IsButtonPressed(*layout, "menu_del"))
        {
            services.sounds.Request(PhotoSound::Decide);
            Show(Stage::AskingToRemove, "menu_del");
            ShowPreview();
        }
        else if (session.IsButtonPressed(*layout, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            session.GoTo(MakeViewScreen(session, fileIndex));
        }
    }

    void StartAdjusting()
    {
        Show(Stage::Adjusting, "menu");
        bool isMovie = session.library.files[fileIndex].IsMovie();
        layout->PaneNamed(PhotoQuestion).isVisible = !isMovie;
        layout->PaneNamed(MovieFrameQuestion).isVisible = isMovie;
        ShowPreview();
    }

    void ShowPreview()
    {
        Formats::RgbaImage icon = CropForPost(photo, IconPictureWidth, IconPictureHeight, middle, zoom);
        preview = std::make_unique<ShownPicture>(session.Services().display.Device(), icon);
    }

    void UpdateAdjusting()
    {
        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*layout, "yes"))
        {
            services.sounds.Request(SaveSound);
            Show(Stage::Saving, "menu_save");
            ShowPreview();
            return;
        }

        if (session.IsButtonPressed(*layout, "no") || session.IsButtonPressed(*layout, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            session.GoTo(MakeViewScreen(session, fileIndex));
            return;
        }

        if (session.IsButtonPressed(*layout, "what"))
        {
            services.sounds.Request(PhotoSound::Decide);
            Show(Stage::AboutPosting, "menu_what");
            return;
        }

        AdjustPart();
    }

    void AdjustPart()
    {
        float newZoom = zoom;
        Formats::Vector2 newMiddle = middle;
        if (session.IsRemoteButtonPressed(Input::RemoteButtons::Plus))
            newZoom = std::min(zoom * ZoomStep, LargestZoom);
        if (session.IsRemoteButtonPressed(Input::RemoteButtons::Minus))
            newZoom = std::max(zoom / ZoomStep, 1.0f);
        if (session.IsRemoteButtonPressed(Input::RemoteButtons::Left))
            newMiddle.x -= MoveStep;
        if (session.IsRemoteButtonPressed(Input::RemoteButtons::Right))
            newMiddle.x += MoveStep;
        if (session.IsRemoteButtonPressed(Input::RemoteButtons::Up))
            newMiddle.y -= MoveStep;
        if (session.IsRemoteButtonPressed(Input::RemoteButtons::Down))
            newMiddle.y += MoveStep;

        newMiddle.x = std::clamp(newMiddle.x, 0.0f, 1.0f);
        newMiddle.y = std::clamp(newMiddle.y, 0.0f, 1.0f);
        if (newZoom == zoom && newMiddle.x == middle.x && newMiddle.y == middle.y)
            return;

        zoom = newZoom;
        middle = newMiddle;
        session.Services().sounds.Request(PhotoSound::Scroll);
        ShowPreview();
    }

    void UpdateSaving()
    {
        if (stepsOfStage < StepsOfSaving)
            return;

        Formats::RgbaImage icon = CropForPost(photo, IconPictureWidth, IconPictureHeight, middle, zoom);
        Formats::RgbaImage channelPreview = CropForPost(photo, PreviewPictureWidth, PreviewPictureHeight, middle, zoom);
        if (session.storage.PostToWiiMenu(icon, channelPreview))
            Show(Stage::Posted, "menu_comp");
        else
            Show(Stage::PostFailed, "menu_ng");
        ShowPreview();
    }

    void StartAskingToPostToBoard()
    {
        Show(Stage::AskingToPostToBoard, "today");
        bool isMovie = session.library.files[fileIndex].IsMovie();
        layout->PaneNamed(PhotoQuestion).isVisible = !isMovie;
        layout->PaneNamed(MovieFrameQuestion).isVisible = isMovie;
        layout->PaneNamed(NewPhotoQuestion).isVisible = false;
    }

    void UpdateAskingToPostToBoard()
    {
        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*layout, "yes"))
        {
            services.sounds.Request(SaveSound);
            Show(Stage::SavingToBoard, "today_save");
        }
        else if (session.IsButtonPressed(*layout, "no") || session.IsButtonPressed(*layout, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            session.GoTo(MakeViewScreen(session, fileIndex));
        }
        else if (session.IsButtonPressed(*layout, "what"))
        {
            services.sounds.Request(PhotoSound::Decide);
            Show(Stage::AboutBoard, "today_what");
        }
    }

    void UpdateSavingToBoard()
    {
        if (stepsOfStage < StepsOfSaving)
            return;

        if (PostToBoard())
            Show(Stage::PostedToBoard, "today_comp");
        else
            Show(Stage::BoardFailed, "today_ng");
    }

    bool PostToBoard()
    {
        std::optional<int> factor = ShrinkFactorToFit(static_cast<int>(photo.width), static_cast<int>(photo.height), BoardPictureLargestWidth,
            BoardPictureLargestHeight);
        Formats::RgbaImage picture = Media::ShrinkPicture(photo, factor.value_or(1));
        Formats::Bytes jpeg = Media::EncodeJpeg(picture, BoardPictureQuality);

        std::uniform_real_distribution<float> share(0.0f, 1.0f);
        Formats::Vector2 place = SystemMenu::PlaceForNewLetter(share(random), share(random));
        try
        {
            SystemMenu::PostPicture(session.BoardStore(), jpeg, place, Now());
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    SystemMenu::LocalTime Now() const
    {
        std::chrono::local_seconds now = session.Services().consoleTime();
        std::chrono::local_days today = std::chrono::floor<std::chrono::days>(now);
        std::chrono::year_month_day date(today);
        std::chrono::hh_mm_ss<std::chrono::seconds> time(now - today);

        SystemMenu::LocalTime localTime;
        localTime.year = static_cast<int>(date.year());
        localTime.month = static_cast<int>(static_cast<unsigned>(date.month()));
        localTime.day = static_cast<int>(static_cast<unsigned>(date.day()));
        localTime.hour = static_cast<int>(time.hours().count());
        localTime.minute = static_cast<int>(time.minutes().count());
        localTime.second = static_cast<int>(time.seconds().count());
        return localTime;
    }

    void UpdateAskingToRemove()
    {
        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*layout, "yes"))
        {
            services.sounds.Request(SaveSound);
            Show(Stage::Removing, "menu_del_save");
        }
        else if (session.IsButtonPressed(*layout, "no") || session.IsButtonPressed(*layout, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            session.GoTo(MakeViewScreen(session, fileIndex));
        }
    }

    void UpdateRemoving()
    {
        if (stepsOfStage < StepsOfSaving)
            return;

        if (session.storage.RemoveWiiMenuPhoto())
            Show(Stage::Removed, "menu_del_comp");
        else
            Show(Stage::RemoveFailed, "menu_del_ng");
    }

    void UpdateAboutPosting()
    {
        if (!session.IsButtonPressed(*layout, "back"))
            return;

        session.Services().sounds.Request(PhotoSound::Cancel);
        StartAdjusting();
    }

    void BackToView(std::string_view button)
    {
        if (!session.IsButtonPressed(*layout, button))
            return;

        session.Services().sounds.Request(PhotoSound::Decide);
        session.GoTo(MakeViewScreen(session, fileIndex));
    }

    PhotoSession& session;
    int fileIndex;
    std::unique_ptr<SystemMenu::MenuLayout> layout;
    Formats::RgbaImage photo;
    std::unique_ptr<ShownPicture> preview;
    Stage stage = Stage::Choosing;
    int stepsOfStage = 0;
    float zoom = 1;
    Formats::Vector2 middle = {0.5f, 0.5f};
    std::mt19937 random{std::random_device{}()};
};

}

std::unique_ptr<PhotoScreen> MakePostScreen(PhotoSession& session, int fileIndex)
{
    return std::make_unique<PostScreen>(session, fileIndex);
}

}
