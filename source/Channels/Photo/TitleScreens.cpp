#include "Channels/Photo/PhotoSession.h"

#include <array>
#include <vector>

// The first screens of the channel: the choice between the SD card and the photos of the
// Wii Message Board, and what the channel says about the card on the way to the thumbnails.

namespace Channels
{

namespace
{

// "Searching the SD Card..." stays at least this long, so that it can be read also when the card is searched at once (Assumed, CHN-31).
constexpr int StepsOfSearchingAtLeast = 45;

constexpr std::array<std::string_view, 4> HowToViewPages = {"title_what1", "title_what2", "title_what3", "title_what4"};

// title_sd_error holds two messages: the card must be formatted (0), and the card cannot be used (1).
constexpr std::string_view UnusableCardMessage = "textTUS1";
constexpr std::string_view CorruptedCardMessage = "textTUS0";

class TitleScreen : public PhotoScreen
{
public:
    explicit TitleScreen(PhotoSession& photoSession)
        : session(photoSession)
        , title(session.MakeLayout("title"))
        , sdCardButton(session.MakeLayout("title_sd0"))
        , messageBoardButton(session.MakeLayout("title_wii0"))
    {
    }

    void Update() override
    {
        title->Calculate();
        sdCardButton->Calculate();
        messageBoardButton->Calculate();

        ChannelServices& services = session.Services();
        if (session.IsButtonPressed(*sdCardButton, "sdR"))
        {
            services.sounds.Request(PhotoSound::Decide);
            session.GoTo(MakeSdCardScreen(session));
        }
        else if (session.IsButtonPressed(*messageBoardButton, "wiiR"))
        {
            services.sounds.Request(PhotoSound::Decide);
            session.GoTo(MakeMessageBoardScreen(session));
        }
        else if (session.IsButtonPressed(*title, "back"))
        {
            services.sounds.Request(PhotoSound::Cancel);
            services.wiiMenu.Request();
        }
    }

    std::string_view Name() const override
    {
        return "Title";
    }

    void Draw() override
    {
        title->Draw();
        sdCardButton->Draw();
        messageBoardButton->Draw();
    }

private:
    PhotoSession& session;
    std::unique_ptr<SystemMenu::MenuLayout> title;
    std::unique_ptr<SystemMenu::MenuLayout> sdCardButton;
    std::unique_ptr<SystemMenu::MenuLayout> messageBoardButton;
};

// The way from "Digital Camera/Cell Phone" to the thumbnails, one layout at a time.
class SdCardScreen : public PhotoScreen
{
public:
    enum class Stage
    {
        NoCard,
        HowToView,
        Searching,
        TooManyFiles,
        NothingFound,
        CardUnusable,
        CardRemoved,
    };

    explicit SdCardScreen(PhotoSession& photoSession)
        : session(photoSession)
    {
        if (IsCardInserted())
            Show(Stage::Searching, "title_sd1");
        else
            Show(Stage::NoCard, "title_sd_not");
    }

    void Update() override
    {
        shown->Calculate();
        ++stepsOfStage;

        if (stage == Stage::NoCard)
            UpdateNoCard();
        else if (stage == Stage::HowToView)
            UpdateHowToView();
        else if (stage == Stage::Searching)
            UpdateSearching();
        else if (stage == Stage::TooManyFiles)
            UpdateTooManyFiles();
        else
            UpdateMessage();
    }

    void Draw() override
    {
        shown->Draw();
    }

    std::string_view Name() const override
    {
        switch (stage)
        {
        case Stage::NoCard:
            return "No SD card";
        case Stage::HowToView:
            return "How do I view photos";
        case Stage::Searching:
            return "Searching the SD card";
        case Stage::TooManyFiles:
            return "Not all files";
        case Stage::NothingFound:
            return "Nothing found";
        case Stage::CardUnusable:
            return "Card not usable";
        case Stage::CardRemoved:
            return "Card removed";
        }
        return "SD card";
    }

private:
    bool IsCardInserted() const
    {
        return session.Services().storage.sdCard.GetStatus().isInserted;
    }

    void Show(Stage newStage, std::string_view layoutName)
    {
        stage = newStage;
        stepsOfStage = 0;
        shown = session.MakeLayout(layoutName);
    }

    void ReturnToTitle()
    {
        session.Services().sounds.Request(PhotoSound::Cancel);
        session.GoTo(MakeTitleScreen(session));
    }

    bool HasCardGone()
    {
        if (IsCardInserted())
            return false;

        Show(Stage::CardRemoved, "title_sd_out");
        return true;
    }

    void UpdateNoCard()
    {
        if (IsCardInserted())
            Show(Stage::Searching, "title_sd1");
        else if (session.IsButtonPressed(*shown, "back"))
            ReturnToTitle();
        else if (session.IsButtonPressed(*shown, "what"))
            ShowHowToViewPage(0);
    }

    void ShowHowToViewPage(int page)
    {
        session.Services().sounds.Request(PhotoSound::Decide);
        howToViewPage = page;
        Show(Stage::HowToView, HowToViewPages[page]);
    }

    void UpdateHowToView()
    {
        if (!session.IsButtonPressed(*shown, "next"))
            return;

        int nextPage = howToViewPage + 1;
        if (nextPage < static_cast<int>(HowToViewPages.size()))
            ShowHowToViewPage(nextPage);
        else
            Show(Stage::NoCard, "title_sd_not");
    }

    void UpdateSearching()
    {
        if (HasCardGone())
            return;

        if (session.IsButtonPressed(*shown, "back"))
        {
            ReturnToTitle();
            return;
        }

        if (stepsOfStage < StepsOfSearchingAtLeast)
            return;

        Ios::Result<std::vector<Ios::SdEntry>> root = session.Services().storage.sdCard.ListDirectory(Ios::SdPath::Root());
        if (root.Failed())
        {
            ShowUnusableCard();
            return;
        }

        session.library = SearchSdCard(session.Services().storage.sdCard);
        if (session.library.files.empty())
            Show(Stage::NothingFound, "title_sd3");
        else if (session.library.HasMoreThanItShows())
            Show(Stage::TooManyFiles, "title_sd2");
        else
            session.GoTo(MakeThumbnailScreen(session, 0));
    }

    void ShowUnusableCard()
    {
        session.Services().sounds.Request(PhotoSound::Warning);
        Show(Stage::CardUnusable, "title_sd_error");
        shown->PaneNamed(CorruptedCardMessage).isVisible = false;
        shown->PaneNamed(UnusableCardMessage).isVisible = true;
    }

    void UpdateTooManyFiles()
    {
        if (HasCardGone())
            return;

        if (session.IsButtonPressed(*shown, "next"))
        {
            session.Services().sounds.Request(PhotoSound::Decide);
            session.GoTo(MakeThumbnailScreen(session, 0));
        }
        else if (session.IsButtonPressed(*shown, "back"))
        {
            ReturnToTitle();
        }
    }

    // Nothing found, an unusable card and a removed card each show a message with one button back to the title.
    void UpdateMessage()
    {
        if (stage == Stage::NothingFound && HasCardGone())
            return;

        if (session.IsButtonPressed(*shown, "back") || session.IsButtonPressed(*shown, "next"))
            ReturnToTitle();
    }

    PhotoSession& session;
    Stage stage = Stage::NoCard;
    int stepsOfStage = 0;
    int howToViewPage = 0;
    std::unique_ptr<SystemMenu::MenuLayout> shown;
};

// "View Wii Message Board photos": the pictures of the letters on the board (the JPEG files of their picture attachments), the oldest
// first, after "Searching the Wii Message Board..." (title_wii1); none gives "The Wii Message Board currently has no photos to display."
// (title_wii3), too many "not all files will be displayed" (title_wii2).
class MessageBoardScreen : public PhotoScreen
{
public:
    explicit MessageBoardScreen(PhotoSession& photoSession)
        : session(photoSession)
        , shown(session.MakeLayout("title_wii1"))
    {
    }

    void Update() override
    {
        shown->Calculate();
        ++steps;

        if (session.IsButtonPressed(*shown, "back"))
        {
            session.Services().sounds.Request(PhotoSound::Cancel);
            session.GoTo(MakeTitleScreen(session));
            return;
        }

        if (hasSearched && session.IsButtonPressed(*shown, "next"))
        {
            session.Services().sounds.Request(PhotoSound::Decide);
            session.GoTo(MakeThumbnailScreen(session, 0));
            return;
        }

        if (steps == StepsOfSearchingAtLeast)
            Search();
    }

    void Draw() override
    {
        shown->Draw();
    }

    std::string_view Name() const override
    {
        return "Message Board photos";
    }

private:
    void Search()
    {
        hasSearched = true;
        session.library = PhotoLibrary();
        SystemMenu::MessageStore& store = session.BoardStore();
        for (SystemMenu::BoardDay day : store.DaysWithRecords())
        {
            for (const SystemMenu::StoredMessage& stored : store.MessagesOfDay(day))
                AddPicturesOf(stored.message);
        }

        if (session.library.files.empty())
            shown = session.MakeLayout("title_wii3");
        else if (session.library.HasMoreThanItShows())
            shown = session.MakeLayout("title_wii2");
        else
            session.GoTo(MakeThumbnailScreen(session, 0));
    }

    void AddPicturesOf(const Formats::BoardMessage& message)
    {
        for (const std::optional<Formats::BoardAttachment>& attachment : message.attachments)
        {
            if (!attachment || attachment->kind != Formats::BoardAttachmentKind::Picture)
                continue;

            ++session.library.foundCount;
            if (session.library.files.size() < ShownFileLimit)
                session.library.files.push_back({Ios::SdPath::Root(), PhotoFileKind::Photo, attachment->contents});
        }
    }

    PhotoSession& session;
    std::unique_ptr<SystemMenu::MenuLayout> shown;
    int steps = 0;
    bool hasSearched = false;
};
}

std::unique_ptr<PhotoScreen> MakeTitleScreen(PhotoSession& session)
{
    return std::make_unique<TitleScreen>(session);
}

std::unique_ptr<PhotoScreen> MakeSdCardScreen(PhotoSession& session)
{
    return std::make_unique<SdCardScreen>(session);
}

std::unique_ptr<PhotoScreen> MakeMessageBoardScreen(PhotoSession& session)
{
    return std::make_unique<MessageBoardScreen>(session);
}

}
