#include "Channels/Photo/PhotoSession.h"
#include "Channels/Photo/SlideShowMusic.h"

#include "Channels/ChannelText.h"
#include "Channels/PagePainter.h"

#include <algorithm>
#include <array>

// The settings of the slide show (layout "slide_set"): Order (Date, Random), Effects (Dramatic, Simple, Nostalgic) and Music, chosen
// from the list of layout "slide_bgm" (the built-in songs, "All AAC files", "Shuffle AAC files" and the AAC files of the card), with
// "Other music..." (slide_what) explaining how to put music on the card. The choice is kept in the save file.

namespace Channels
{

namespace
{

constexpr std::array<std::string_view, 2> OrderTexts = {"a_setTUS0", "a_setTUS1"};
constexpr std::array<std::string_view, 3> EffectTexts = {"c_setTUS0", "c_setTUS1", "c_setTUS2"};
constexpr std::string_view SongText = "d_setTUS";
constexpr std::string_view ListTitleWithLimit = "bgmTUS1";
constexpr std::string_view ListTitle = "bgmTUS0";
constexpr std::string_view UnsupportedMessage = "slide_bgm_error";

// The list of songs between the belt and the buttons, in page units of the 4:3 layout (Assumed, CHN-33).
constexpr float ListTop = 60;
constexpr float ListLeft = 120;
constexpr float RowHeight = 34;
constexpr float RowWidth = 368;
constexpr int RowsShown = 8;
constexpr float SongTextSize = 22;
constexpr Formats::Rgba8Color SongColor = {40, 40, 40, 255};
constexpr Formats::Rgba8Color ChosenSongColor = {20, 110, 220, 255};

class SlideSettingsScreen : public PhotoScreen
{
public:
    SlideSettingsScreen(PhotoSession& photoSession, int index)
        : session(photoSession)
        , fileIndex(index)
        , settingsLayout(session.MakeLayout("slide_set"))
        , songsLayout(session.MakeLayout("slide_bgm"))
        , aboutMusic(session.MakeLayout("slide_what"))
        , unsupported(session.MakeLayout(UnsupportedMessage))
        , painter(session.Services().display.Device(), session.Font())
        , songs(ListSlideShowSongs(session.library.musicFiles))
    {
        songsLayout->PaneNamed(ListTitle).isVisible = false;
        songsLayout->PaneNamed(ListTitleWithLimit).isVisible = true;
        ShowSettings();
    }

    void Update() override
    {
        for (SystemMenu::MenuLayout* layout : {settingsLayout.get(), songsLayout.get(), aboutMusic.get(), unsupported.get()})
            layout->Calculate();

        if (isShowingAboutMusic)
            UpdateAboutMusic();
        else if (isShowingUnsupported)
            UpdateUnsupported();
        else if (isChoosingSong)
            UpdateSongList();
        else
            UpdateSettings();
    }

    void Draw() override
    {
        if (isShowingAboutMusic)
        {
            aboutMusic->Draw();
            return;
        }

        if (!isChoosingSong)
        {
            settingsLayout->Draw();
            return;
        }

        songsLayout->Draw();
        DrawSongList();
        if (isShowingUnsupported)
            unsupported->Draw();
    }

    std::string_view Name() const override
    {
        if (isShowingAboutMusic)
            return "How can I listen to my own music?";
        if (isShowingUnsupported)
            return "AAC not supported";
        return isChoosingSong ? "Choose a song" : "Slide-show Settings";
    }

private:
    PhotoSettings& Settings()
    {
        return session.storage.Settings();
    }

    void ShowSettings()
    {
        for (std::size_t order = 0; order < OrderTexts.size(); ++order)
            settingsLayout->PaneNamed(OrderTexts[order]).isVisible = static_cast<int>(order) == static_cast<int>(Settings().order);

        for (std::size_t effect = 0; effect < EffectTexts.size(); ++effect)
            settingsLayout->PaneNamed(EffectTexts[effect]).isVisible = static_cast<int>(effect) == static_cast<int>(Settings().effect);

        int song = std::clamp(Settings().song, 0, static_cast<int>(songs.size()) - 1);
        if (settingsLayout->FindPane(SongText) != nullptr)
            settingsLayout->SetText(SongText, songs[song].name);
    }

    void UpdateSettings()
    {
        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*settingsLayout, "a_btn"))
        {
            services.sounds.Request(PhotoSound::Decide);
            Settings().order = Settings().order == SlideOrder::Date ? SlideOrder::Random : SlideOrder::Date;
            SaveAndShow();
        }
        else if (session.IsButtonPressed(*settingsLayout, "c_btn"))
        {
            services.sounds.Request(PhotoSound::Decide);
            int nextEffect = (static_cast<int>(Settings().effect) + 1) % static_cast<int>(EffectTexts.size());
            Settings().effect = static_cast<SlideEffect>(nextEffect);
            SaveAndShow();
        }
        else if (session.IsButtonPressed(*settingsLayout, "d_btn"))
        {
            services.sounds.Request(PhotoSound::Decide);
            isChoosingSong = true;
        }
        else if (session.IsButtonPressed(*settingsLayout, "start"))
        {
            services.sounds.Request(PhotoSound::Decide);
            session.GoTo(MakeSlideShowScreen(session, fileIndex));
        }
    }

    void SaveAndShow()
    {
        session.storage.SaveSettings();
        ShowSettings();
    }

    PageRectangle RowBox(int row) const
    {
        return {ListLeft, ListTop + row * RowHeight, RowWidth, RowHeight};
    }

    void DrawSongList()
    {
        for (int row = 0; row < RowsShown && firstShownSong + row < static_cast<int>(songs.size()); ++row)
        {
            int song = firstShownSong + row;
            PageTextStyle style;
            style.size = SongTextSize;
            style.color = song == Settings().song ? ChosenSongColor : SongColor;
            painter.DrawText(songs[song].name, style, RowBox(row));
        }
    }

    void UpdateSongList()
    {
        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*songsLayout, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            isChoosingSong = false;
            return;
        }

        if (session.IsButtonPressed(*songsLayout, "what"))
        {
            services.sounds.Request(PhotoSound::Decide);
            isShowingAboutMusic = true;
            return;
        }

        int lastFirst = std::max(0, static_cast<int>(songs.size()) - RowsShown);
        if (session.IsButtonPressed(*songsLayout, "up") && firstShownSong > 0)
        {
            services.sounds.Request(PhotoSound::Scroll);
            --firstShownSong;
        }
        else if (session.IsButtonPressed(*songsLayout, "down") && firstShownSong < lastFirst)
        {
            services.sounds.Request(PhotoSound::Scroll);
            ++firstShownSong;
        }
        else
        {
            ChoosePressedSong();
        }
    }

    void ChoosePressedSong()
    {
        std::optional<Formats::Vector2> place = session.PlaceOfPress();
        if (!place)
            return;

        Formats::Vector2 pagePoint = painter.PictureToPage(*place);
        for (int row = 0; row < RowsShown && firstShownSong + row < static_cast<int>(songs.size()); ++row)
        {
            if (!RowBox(row).Contains(pagePoint))
                continue;

            int song = firstShownSong + row;
            if (!IsSongPlayable(session, songs[song]))
            {
                session.Services().sounds.Request(PhotoSound::Warning);
                isShowingUnsupported = true;
                return;
            }

            session.Services().sounds.Request(PhotoSound::Decide);
            Settings().song = song;
            SaveAndShow();
            isChoosingSong = false;
            return;
        }
    }

    void UpdateUnsupported()
    {
        if (!session.IsButtonPressed(*unsupported, "back"))
            return;

        session.Services().sounds.Request(PhotoSound::Cancel);
        isShowingUnsupported = false;
    }

    void UpdateAboutMusic()
    {
        if (!session.IsButtonPressed(*aboutMusic, "back"))
            return;

        session.Services().sounds.Request(PhotoSound::Cancel);
        isShowingAboutMusic = false;
    }

    PhotoSession& session;
    int fileIndex;
    std::unique_ptr<SystemMenu::MenuLayout> settingsLayout;
    std::unique_ptr<SystemMenu::MenuLayout> songsLayout;
    std::unique_ptr<SystemMenu::MenuLayout> aboutMusic;
    std::unique_ptr<SystemMenu::MenuLayout> unsupported;
    PagePainter painter;
    std::vector<SlideShowSong> songs;
    int firstShownSong = 0;
    bool isChoosingSong = false;
    bool isShowingAboutMusic = false;
    bool isShowingUnsupported = false;
};

}

std::unique_ptr<PhotoScreen> MakeSlideSettingsScreen(PhotoSession& session, int fileIndex)
{
    return std::make_unique<SlideSettingsScreen>(session, fileIndex);
}

}
