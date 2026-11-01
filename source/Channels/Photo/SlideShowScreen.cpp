#include "Channels/Photo/PhotoPictures.h"
#include "Channels/Photo/PhotoSession.h"
#include "Channels/Photo/ShownPicture.h"

#include <algorithm>

// The slide show: the photos one after the other with a cross fade and the built-in music, from the photo it was started at and
// round again. A or B stops it and shows the layout "slide_pause" with Resume slide show and Back.

namespace Channels
{

namespace
{

// The pace of the slide show "Simple" (Assumed, CHN-29). The settings of the original (order, effects, the user's music) are not
// built yet, so their button is not shown.
constexpr double SecondsPerSlide = 5;
constexpr double SecondsOfCrossFade = 1;
constexpr std::string_view SettingsButton = "set";

class SlideShowScreen : public PhotoScreen
{
public:
    SlideShowScreen(PhotoSession& photoSession, int firstFileIndex)
        : session(photoSession)
        , pause(session.MakeLayout("slide_pause"))
    {
        pause->PaneNamed(SettingsButton).isVisible = false;
        for (int index = 0; index < static_cast<int>(session.library.files.size()); ++index)
        {
            if (!session.library.files[index].IsMovie())
                photoIndexes.push_back(index);
        }

        auto first = std::lower_bound(photoIndexes.begin(), photoIndexes.end(), firstFileIndex);
        position = first == photoIndexes.end() ? 0 : static_cast<int>(first - photoIndexes.begin());
        session.Services().sounds.Request(PhotoSound::SlideShowMusic);
        StartSlide();
    }

    void Update() override
    {
        pause->Calculate();
        if (isPaused)
        {
            UpdatePause();
            return;
        }

        ChannelServices& services = session.Services();
        bool wantsToPause = session.IsRemoteButtonPressed(Input::RemoteButtons::A) || session.IsRemoteButtonPressed(Input::RemoteButtons::B);
        if (wantsToPause && services.fader.GetStatus() == SystemMenu::FaderStatus::Clear)
        {
            services.sounds.Request(PhotoSound::Decide);
            isPaused = true;
            pausedAtSeconds = SecondsOfSlide();
            return;
        }

        if (!photoIndexes.empty() && SecondsOfSlide() >= SecondsPerSlide)
        {
            position = (position + 1) % static_cast<int>(photoIndexes.size());
            StartSlide();
        }
    }

    void Draw() override
    {
        Graphics::Canvas canvas = session.Services().display.GetCanvas();
        PictureBox wholeScreen = {0, 0, canvas.width, canvas.height};
        float fadeIn = static_cast<float>(std::min(1.0, SecondsOfSlide() / SecondsOfCrossFade));
        if (previous)
            previous->DrawInside(wholeScreen, 1 - fadeIn);
        if (current)
            current->DrawInside(wholeScreen, fadeIn);

        if (isPaused)
            pause->Draw();
    }

    std::string_view Name() const override
    {
        return isPaused ? "Slide show paused" : "Slide show";
    }

private:
    int ShownFileIndex() const
    {
        return photoIndexes.empty() ? -1 : photoIndexes[position];
    }

    double SecondsOfSlide() const
    {
        if (isPaused)
            return pausedAtSeconds;

        return session.Services().clock.Seconds() - slideStartSeconds;
    }

    void StartSlide()
    {
        slideStartSeconds = session.Services().clock.Seconds();
        previous = std::move(current);
        if (photoIndexes.empty())
            return;

        const PhotoFile& file = session.library.files[photoIndexes[position]];
        std::optional<Formats::Bytes> bytes = ReadCardFile(session.Services().storage.sdCard, file.path);
        std::optional<Formats::RgbaImage> photo;
        if (bytes)
            photo = MakeShownPhoto(*bytes);
        if (photo)
            current = std::make_unique<ShownPicture>(session.Services().display.Device(), *photo);
    }

    void UpdatePause()
    {
        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*pause, "continue"))
        {
            services.sounds.Request(PhotoSound::Decide);
            isPaused = false;
            slideStartSeconds = services.clock.Seconds() - pausedAtSeconds;
        }
        else if (session.IsButtonPressed(*pause, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            services.sounds.Request(PhotoSound::MainMusic);
            session.GoTo(MakeThumbnailScreen(session, std::max(ShownFileIndex(), 0)));
        }
    }

    PhotoSession& session;
    std::unique_ptr<SystemMenu::MenuLayout> pause;
    std::vector<int> photoIndexes;
    int position = 0;
    std::unique_ptr<ShownPicture> previous;
    std::unique_ptr<ShownPicture> current;
    double slideStartSeconds = 0;
    double pausedAtSeconds = 0;
    bool isPaused = false;
};

}

std::unique_ptr<PhotoScreen> MakeSlideShowScreen(PhotoSession& session, int firstFileIndex)
{
    return std::make_unique<SlideShowScreen>(session, firstFileIndex);
}

}
