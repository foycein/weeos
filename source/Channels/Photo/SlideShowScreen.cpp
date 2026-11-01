#include "Channels/Photo/PhotoPictures.h"
#include "Channels/Photo/PhotoSession.h"
#include "Channels/Photo/ShownPicture.h"
#include "Channels/Photo/SlideShowMusic.h"

#include <algorithm>
#include <random>

// The slide show: the photos one after the other with a cross fade and the built-in music, from the photo it was started at and
// round again. A or B stops it and shows the layout "slide_pause" with Resume slide show and Back.

namespace Channels
{

namespace
{

// The pace and the look of the three effects (Assumed, CHN-31 and CHN-33): Simple fades from one photo to the next, Dramatic moves
// slowly closer to each photo and fades faster, Nostalgic shows the photos in the brown of old prints and fades slowly.
constexpr double SecondsPerSlide = 5;
constexpr double SecondsOfSimpleFade = 1;
constexpr double SecondsOfDramaticFade = 0.5;
constexpr double SecondsOfNostalgicFade = 2;
constexpr float DramaticLargestZoom = 1.15f;
constexpr float SepiaRed = 1.07f;
constexpr float SepiaGreen = 0.88f;
constexpr float SepiaBlue = 0.66f;

double SecondsOfFade(SlideEffect effect)
{
    if (effect == SlideEffect::Dramatic)
        return SecondsOfDramaticFade;

    if (effect == SlideEffect::Nostalgic)
        return SecondsOfNostalgicFade;

    return SecondsOfSimpleFade;
}

Formats::RgbaImage MakeSepia(const Formats::RgbaImage& photo)
{
    Formats::RgbaImage sepia = photo;
    for (Formats::Rgba8Pixel& pixel : sepia.pixels)
    {
        float gray = 0.299f * pixel.red + 0.587f * pixel.green + 0.114f * pixel.blue;
        pixel.red = static_cast<Formats::Byte>(std::min(255.0f, gray * SepiaRed));
        pixel.green = static_cast<Formats::Byte>(std::min(255.0f, gray * SepiaGreen));
        pixel.blue = static_cast<Formats::Byte>(std::min(255.0f, gray * SepiaBlue));
    }
    return sepia;
}

class SlideShowScreen : public PhotoScreen
{
public:
    SlideShowScreen(PhotoSession& photoSession, int firstFileIndex)
        : session(photoSession)
        , pause(session.MakeLayout("slide_pause"))
    {
        for (int index = 0; index < static_cast<int>(session.library.files.size()); ++index)
        {
            if (!session.library.files[index].IsMovie())
                photoIndexes.push_back(index);
        }

        auto first = std::lower_bound(photoIndexes.begin(), photoIndexes.end(), firstFileIndex);
        position = first == photoIndexes.end() ? 0 : static_cast<int>(first - photoIndexes.begin());

        const PhotoSettings& settings = session.storage.Settings();
        effect = settings.effect;
        if (settings.order == SlideOrder::Random)
        {
            std::mt19937 random(std::random_device{}());
            std::shuffle(photoIndexes.begin(), photoIndexes.end(), random);
            position = 0;
        }

        StartMusic(settings.song);
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
        float fadeIn = static_cast<float>(std::min(1.0, SecondsOfSlide() / SecondsOfFade(effect)));
        float zoom = 1;
        if (effect == SlideEffect::Dramatic)
            zoom = 1 + (DramaticLargestZoom - 1) * static_cast<float>(std::min(1.0, SecondsOfSlide() / SecondsPerSlide));

        if (previous)
            previous->DrawInside(wholeScreen, 1 - fadeIn, previousZoom);
        if (current)
            current->DrawInside(wholeScreen, fadeIn, zoom);

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

    void StartMusic(int songIndex)
    {
        std::vector<SlideShowSong> songs = ListSlideShowSongs(session.library.musicFiles);
        if (songIndex < 0 || songIndex >= static_cast<int>(songs.size()))
            return;

        const SlideShowSong& song = songs[songIndex];
        if (song.kind == SongKind::BuiltIn)
            session.Services().sounds.Request(song.soundName);
    }

    void StartSlide()
    {
        previousZoom = effect == SlideEffect::Dramatic ? DramaticLargestZoom : 1;
        slideStartSeconds = session.Services().clock.Seconds();
        previous = std::move(current);
        if (photoIndexes.empty())
            return;

        std::optional<Formats::RgbaImage> photo = session.PhotoOfFile(photoIndexes[position]);
        if (photo && effect == SlideEffect::Nostalgic)
            photo = MakeSepia(*photo);
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
        else if (session.IsButtonPressed(*pause, "set"))
        {
            services.sounds.Request(PhotoSound::Decide);
            session.GoTo(MakeSlideSettingsScreen(session, std::max(ShownFileIndex(), 0)));
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
    SlideEffect effect = SlideEffect::Simple;
    float previousZoom = 1;
};

}

std::unique_ptr<PhotoScreen> MakeSlideShowScreen(PhotoSession& session, int firstFileIndex)
{
    return std::make_unique<SlideShowScreen>(session, firstFileIndex);
}

}
