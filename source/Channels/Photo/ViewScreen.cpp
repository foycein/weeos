#include "Channels/Photo/PhotoPictures.h"
#include "Channels/Photo/PhotoSession.h"
#include "Channels/Photo/ShownPicture.h"

#include "Media/MovieFrames.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <array>

// One photo or one movie on the whole screen (layout "view"): Previous and Next (also Left and Right), the turn button, the zoom
// buttons (+ and -) of a photo, Play again and Play backward at the end of a movie, Back to the list and the slide show.

namespace Channels
{

namespace
{

// How far the zoom buttons magnify a photo (Assumed, CHN-31).
constexpr std::array<float, 3> ZoomLevels = {1, 2, 4};

// The panes of the layout "error": its messages for a photo and a movie whose format is not supported.
constexpr std::string_view UnsupportedPhotoMessage = "textTUS0A";
constexpr std::string_view UnsupportedMovieMessage = "textTUS1A";
constexpr std::string_view OtherUnsupportedMessage = "textTUS2A";

// The buttons of a feature of the view that WeeOS does not have yet: they are not shown (Fun!, Post).
constexpr std::array<std::string_view, 2> ButtonsNotBuilt = {"today", "play"};
constexpr std::array<std::string_view, 2> ButtonsAtTheEndOfAMovie = {"again", "playback"};
constexpr std::array<std::string_view, 3> ButtonsOfAPhotoOnly = {"rot", "zoom_in", "zoom_out"};

class ViewScreen : public PhotoScreen
{
public:
    ViewScreen(PhotoSession& photoSession, int fileIndex)
        : session(photoSession)
        , buttons(session.MakeLayout("view"))
        , unsupported(session.MakeLayout("error"))
    {
        for (std::string_view button : ButtonsNotBuilt)
            buttons->PaneNamed(button).isVisible = false;

        Open(fileIndex);
    }

    void Update() override
    {
        buttons->Calculate();
        unsupported->Calculate();

        if (movie)
            UpdateMovie();

        UpdateButtons();
    }

    void Draw() override
    {
        if (shown)
        {
            Graphics::Canvas canvas = session.Services().display.GetCanvas();
            shown->DrawInside({0, 0, canvas.width, canvas.height}, 1, ZoomLevels[zoom]);
        }

        if (isUnsupported)
            unsupported->Draw();

        buttons->Draw();
    }

    std::string_view Name() const override
    {
        if (isUnsupported)
            return "Not supported";

        return movie ? "Movie" : "Photo";
    }

private:
    const PhotoFile& File() const
    {
        return session.library.files[index];
    }

    void Open(int fileIndex)
    {
        index = fileIndex;
        shown = nullptr;
        movie = std::nullopt;
        fileBytes.clear();
        zoom = 0;
        quarterTurns = 0;
        isUnsupported = false;

        std::optional<Formats::Bytes> bytes = ReadCardFile(session.Services().storage.sdCard, File().path);
        if (bytes)
            fileBytes = std::move(*bytes);

        if (File().IsMovie())
            OpenMovie();
        else
            OpenPhoto();

        ShowButtonsOfTheFile();
    }

    void OpenPhoto()
    {
        std::optional<Formats::RgbaImage> photo = MakeShownPhoto(fileBytes);
        if (!photo)
        {
            ShowUnsupported(UnsupportedPhotoMessage);
            return;
        }

        photoPicture = *photo;
        ShowPhoto();
    }

    void ShowPhoto()
    {
        shown = std::make_unique<ShownPicture>(session.Services().display.Device(), TurnByQuarterTurns(photoPicture, quarterTurns));
    }

    void OpenMovie()
    {
        movie = ReadPlayableMovie(fileBytes, File().kind);
        if (!movie || movie->video->frames.empty())
        {
            movie = std::nullopt;
            ShowUnsupported(UnsupportedMovieMessage);
            return;
        }
        StartMovie(false);
    }

    void ShowUnsupported(std::string_view message)
    {
        isUnsupported = true;
        unsupported->PaneNamed(UnsupportedPhotoMessage).isVisible = message == UnsupportedPhotoMessage;
        unsupported->PaneNamed(UnsupportedMovieMessage).isVisible = message == UnsupportedMovieMessage;
        unsupported->PaneNamed(OtherUnsupportedMessage).isVisible = false;
    }

    void ShowButtonsOfTheFile()
    {
        for (std::string_view button : ButtonsOfAPhotoOnly)
            buttons->PaneNamed(button).isVisible = !File().IsMovie() && !isUnsupported;

        for (std::string_view button : ButtonsAtTheEndOfAMovie)
            buttons->PaneNamed(button).isVisible = false;
    }

    void StartMovie(bool backward)
    {
        isPlayingBackward = backward;
        movieStartSeconds = session.Services().clock.Seconds();
        shownFrame = -1;
        for (std::string_view button : ButtonsAtTheEndOfAMovie)
            buttons->PaneNamed(button).isVisible = false;
    }

    // The frame whose time has come: forward from the first frame, or backward from the last.
    int FrameAtTime(double seconds) const
    {
        const std::vector<double>& starts = movie->video->frameStartSeconds;
        double time = isPlayingBackward ? movie->durationSeconds - seconds : seconds;
        auto later = std::upper_bound(starts.begin(), starts.end(), time);
        int frame = static_cast<int>(later - starts.begin()) - 1;
        return std::clamp(frame, 0, static_cast<int>(starts.size()) - 1);
    }

    void UpdateMovie()
    {
        double seconds = session.Services().clock.Seconds() - movieStartSeconds;
        if (seconds >= movie->durationSeconds)
        {
            for (std::string_view button : ButtonsAtTheEndOfAMovie)
                buttons->PaneNamed(button).isVisible = true;
            return;
        }

        int frame = FrameAtTime(seconds);
        if (frame == shownFrame)
            return;

        shownFrame = frame;
        ShowMovieFrame(frame);
    }

    // A frame that cannot be decoded leaves the last good one on the screen.
    void ShowMovieFrame(int frame)
    {
        const Media::MoviePiece& piece = movie->video->frames[frame];
        if (piece.size == 0 || !Formats::IsRangeInside(piece.place, piece.size, fileBytes.size()))
            return;

        try
        {
            Formats::ByteView frameBytes = Formats::ByteView(fileBytes).subspan(static_cast<std::size_t>(piece.place), piece.size);
            Formats::RgbaImage picture = Media::DecodeMovieFrame(*movie->video, frameBytes);
            shown = std::make_unique<ShownPicture>(session.Services().display.Device(), picture);
        }
        catch (const Formats::FormatError&)
        {
        }
    }

    void UpdateButtons()
    {
        ChannelServices& services = session.Services();
        int fileCount = static_cast<int>(session.library.files.size());

        if (session.IsButtonPressed(*buttons, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            session.GoTo(MakeThumbnailScreen(session, index));
        }
        else if (session.IsButtonPressed(*buttons, "slide"))
        {
            services.sounds.Request(PhotoSound::Decide);
            session.GoTo(MakeSlideShowScreen(session, index));
        }
        else if (session.IsButtonPressed(*buttons, "next") || session.IsRemoteButtonPressed(Input::RemoteButtons::Right))
        {
            services.sounds.Request(PhotoSound::NextPhoto);
            Open((index + 1) % fileCount);
        }
        else if (session.IsButtonPressed(*buttons, "prev") || session.IsRemoteButtonPressed(Input::RemoteButtons::Left))
        {
            services.sounds.Request(PhotoSound::NextPhoto);
            Open((index + fileCount - 1) % fileCount);
        }
        else if (session.IsButtonPressed(*buttons, "rot"))
        {
            services.sounds.Request(PhotoSound::Turn);
            ++quarterTurns;
            ShowPhoto();
        }
        else if (session.IsButtonPressed(*buttons, "zoom_in") || (!File().IsMovie() && session.IsRemoteButtonPressed(Input::RemoteButtons::Plus)))
        {
            ChangeZoom(1, PhotoSound::ZoomIn);
        }
        else if (session.IsButtonPressed(*buttons, "zoom_out") || (!File().IsMovie() && session.IsRemoteButtonPressed(Input::RemoteButtons::Minus)))
        {
            ChangeZoom(-1, PhotoSound::ZoomOut);
        }
        else if (session.IsButtonPressed(*buttons, "again"))
        {
            services.sounds.Request(PhotoSound::Decide);
            StartMovie(false);
        }
        else if (session.IsButtonPressed(*buttons, "playback"))
        {
            services.sounds.Request(PhotoSound::Decide);
            StartMovie(true);
        }
    }

    void ChangeZoom(int step, std::string_view sound)
    {
        if (isUnsupported)
            return;

        int newZoom = std::clamp(zoom + step, 0, static_cast<int>(ZoomLevels.size()) - 1);
        if (newZoom == zoom)
            return;

        zoom = newZoom;
        session.Services().sounds.Request(sound);
    }

    PhotoSession& session;
    std::unique_ptr<SystemMenu::MenuLayout> buttons;
    std::unique_ptr<SystemMenu::MenuLayout> unsupported;
    int index = 0;
    Formats::Bytes fileBytes;
    Formats::RgbaImage photoPicture;
    std::unique_ptr<ShownPicture> shown;
    std::optional<Media::Movie> movie;
    double movieStartSeconds = 0;
    int shownFrame = -1;
    bool isPlayingBackward = false;
    bool isUnsupported = false;
    int zoom = 0;
    int quarterTurns = 0;
};

}

std::unique_ptr<PhotoScreen> MakeViewScreen(PhotoSession& session, int fileIndex)
{
    return std::make_unique<ViewScreen>(session, fileIndex);
}

}
