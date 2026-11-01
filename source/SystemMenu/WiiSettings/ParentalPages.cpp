#include "SystemMenu/WiiSettings/ParentalPages.h"

#include "Settings/ParentalMasterKey.h"

#include <algorithm>
#include <array>

namespace SystemMenu
{

namespace
{

constexpr std::u16string_view Title = u"Parental Controls";
constexpr std::u16string_view OtherSettingsTitle = u"Other Settings";

constexpr std::size_t PinLength = 4;
constexpr std::size_t LongestAnswer = 32;
constexpr std::size_t MasterKeyLength = 5;

// The request above a field or a question, below the tabs.
constexpr PageRectangle RequestBounds = {24, 84, 560, 44};

// The six secret questions of the pages (_msgParentalString of FIX/js/US/ENG/iplMessage.js), in the order of their numbers.
constexpr auto SecretQuestions = std::to_array<std::u16string_view>({
    u"What is your mother's maiden name?",
    u"What is your place of birth?",
    u"What is your favorite sports team?",
    u"What was your favorite birthday present?",
    u"What's your favorite movie?",
    u"If you could go anywhere, where would you go?",
});

// A rating of the ESRB that the user can choose as the highest one allowed: the age the console keeps for it, the words and the
// picture of its page (ESRB01 to ESRB06 of the pages, the numbers of their script).
struct Rating
{
    std::uint8_t age;
    std::u16string_view name;
    std::u16string_view meaning;
    std::string_view picture;
};

constexpr auto Ratings = std::to_array<Rating>({
    {20, u"ALLOW ALL VIDEO GAME CONTENT", u"", "FIX/COMMON/ESRB/No_Rating_eng.gif"},
    {17, u"MATURE", u"Suitable for ages 17 and older.", "FIX/COMMON/ESRB/M.gif"},
    {13, u"TEEN", u"Suitable for ages 13 and older.", "FIX/COMMON/ESRB/T.gif"},
    {10, u"EVERYONE 10+", u"Suitable for ages 10 and older.", "FIX/COMMON/ESRB/E10.gif"},
    {6, u"EVERYONE", u"Suitable for ages 6 and older.", "FIX/COMMON/ESRB/E.gif"},
    {3, u"EARLY CHILDHOOD", u"Suitable for ages 3 and older.", "FIX/COMMON/ESRB/eC.gif"},
});

constexpr PageRectangle RatingNameBounds = {24, 84, 560, 44};
constexpr PageRectangle RatingMeaningBounds = {24, 250, 560, 44};
constexpr PageRectangle RatingPictureBounds = {280, 150, 49, 72};
constexpr PageRectangle WideRatingPictureBounds = {232, 170, 144, 32};
constexpr int RatingArrowLeft = 518;
constexpr int RatingArrowUpTop = 100;
constexpr int RatingArrowDownTop = 220;

// The restrictions of the page "Other Settings", one after the other.
enum class Restriction
{
    InternetChannel,
    WiiShopChannel,
    NewsChannel,
};

constexpr auto RestrictionNames = std::to_array<std::u16string_view>({
    u"Internet Channel",
    u"Wii Shop Channel",
    u"News Channel",
});

constexpr int RestrictionCount = static_cast<int>(RestrictionNames.size());

bool& RestrictionFlag(Settings::ParentalRestrictions& restrictions, int restriction)
{
    switch (static_cast<Restriction>(restriction))
    {
    case Restriction::InternetChannel:
        return restrictions.isInternetChannelLocked;

    case Restriction::WiiShopChannel:
        return restrictions.isShoppingLocked;

    default:
        return restrictions.isNewsChannelLocked;
    }
}

int FindRating(std::uint8_t age)
{
    for (std::size_t index = 0; index < Ratings.size(); ++index)
    {
        if (Ratings[index].age == age)
            return static_cast<int>(index);
    }

    return 0;
}

std::u16string_view QuestionText(std::uint8_t question)
{
    if (question >= SecretQuestions.size())
        return SecretQuestions.front();

    return SecretQuestions[question];
}

std::u16string StarsFor(const std::u16string& digits)
{
    return std::u16string(digits.size(), u'*');
}

std::u16string PinAsText(const std::string& pin)
{
    return std::u16string(pin.begin(), pin.end());
}

std::string PinFromText(const std::u16string& text)
{
    std::string pin;
    for (char16_t character : text)
        pin.push_back(static_cast<char>(character));

    return pin;
}

bool IsWholePin(const std::u16string& text)
{
    if (text.size() != PinLength)
        return false;

    for (char16_t character : text)
    {
        if (character < u'0' || character > u'9')
            return false;
    }

    return true;
}

}

ParentalPages::ParentalPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings, const Settings::ConsoleClock& consoleClock,
    Ios::NetworkConfigService& networkConfigService)
    : navigator(pageNavigator)
    , settings(systemSettings)
    , clock(consoleClock)
    , networkConfig(networkConfigService)
{
}

bool ParentalPages::Owns(PageId page) const
{
    return page >= PageId::ParentalUseQuestion && page <= PageId::ParentalDisabled;
}

void ParentalPages::EnterPage(PageId page)
{
    switch (page)
    {
    case PageId::ParentalUseQuestion:
    case PageId::ParentalChangeQuestion:
        draft = settings.GetParentalControls();
        draftRestrictions = settings.GetParentalRestrictions();
        isChangingOneSetting = false;
        break;

    case PageId::ParentalPinChoice:
        typedPin.clear();
        break;

    case PageId::ParentalPinRepeat:
        typedRepeat.clear();
        break;

    case PageId::ParentalAnswerChoice:
        typedAnswer = draft.secretAnswer;
        break;

    case PageId::ParentalSettingsOverview:
        isChangingOneSetting = true;
        break;

    case PageId::ParentalSettingsMenu:
        isChangingOneSetting = false;
        break;

    case PageId::ParentalRatingChoice:
        shownRating = FindRating(draft.highestAllowedAge);
        break;

    case PageId::ParentalPinCheck:
    case PageId::ParentalAnswerCheck:
    case PageId::ParentalMasterKeyEntry:
        typedCheck.clear();
        break;

    default:
        break;
    }
}

void ParentalPages::BuildPage(PageId page, Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, std::u16string(Title));

    // PageId lists the pages of the parental controls in three groups: the setup, the settings, and the checks of controls that are on.
    if (page <= PageId::ParentalAnswerChoice)
        BuildSetupPage(page, builder);
    else if (page <= PageId::ParentalCleared)
        BuildSettingsPage(page, builder);
    else
        BuildCheckPage(page, builder);
}

void ParentalPages::BuildSetupPage(PageId page, PageBuilder& builder)
{
    switch (page)
    {
    case PageId::ParentalUseQuestion:
        builder.AddMessage({u"Use Parental Controls?"});
        builder.AddBottomLeft(u"Yes", [this] { navigator.GoTo(PageId::ParentalDescription); });
        builder.AddBottomRight(u"No", [this] { navigator.GoTo(PageId::SecondIndex); }).sound = PageSound::Back;
        break;

    case PageId::ParentalDescription:
        builder.AddMessage({u"The Parental Controls feature", u"allows you to control access to", u"certain software and Internet",
            u"content. For details, please refer", u"to the Wii Operations Manual."});
        builder.AddBackButton([this] { navigator.GoTo(PageId::ParentalUseQuestion); });
        builder.AddBottomRight(u"OK", [this] { navigator.GoTo(PageId::ParentalGuardianNote); });
        break;

    case PageId::ParentalGuardianNote:
        builder.AddMessage({u"This setting should be", u"implemented by parents", u"or guardians."});
        builder.AddBackButton([this] { navigator.GoTo(PageId::ParentalDescription); });
        builder.AddBottomRight(u"OK", [this] { navigator.GoTo(PageId::ParentalPinChoice); });
        break;

    case PageId::ParentalPinChoice:
        BuildPinChoice(builder);
        break;

    case PageId::ParentalPinRepeat:
        BuildPinRepeat(builder);
        break;

    case PageId::ParentalPinMismatch:
        BuildNotice(builder, {u"This PIN is incorrect.", u"Please try again."}, PageId::ParentalPinChoice);
        break;

    case PageId::ParentalQuestionChoice:
        BuildQuestionChoice(builder);
        break;

    default:
        BuildAnswerChoice(builder);
        break;
    }
}

void ParentalPages::BuildSettingsPage(PageId page, PageBuilder& builder)
{
    switch (page)
    {
    case PageId::ParentalSettingsMenu:
        BuildSettingsMenu(builder);
        break;

    case PageId::ParentalSettingsOverview:
        BuildSettingsOverview(builder);
        break;

    case PageId::ParentalRatingChoice:
        BuildRatingChoice(builder);
        break;

    case PageId::ParentalRestriction:
        BuildRestriction(builder);
        break;

    case PageId::ParentalClearQuestion:
        builder.AddMessage({u"Clear settings?"});
        builder.AddBottomLeft(u"Cancel", [this] { navigator.GoTo(PageId::ParentalSettingsMenu); }).sound = PageSound::Back;
        builder.AddBottomRight(u"OK", [this] { ClearSettings(); });
        break;

    default:
        BuildNotice(builder, {u"Settings have been cleared."}, PageId::SecondIndex);
        break;
    }
}

void ParentalPages::BuildCheckPage(PageId page, PageBuilder& builder)
{
    switch (page)
    {
    case PageId::ParentalChangeQuestion:
        builder.AddMessage({u"Change Parental Controls?"});
        builder.AddBottomLeft(u"Yes", [this] { navigator.GoTo(PageId::ParentalPinCheck); });
        builder.AddBottomRight(u"No", [this] { navigator.GoTo(PageId::SecondIndex); }).sound = PageSound::Back;
        break;

    case PageId::ParentalPinCheck:
        BuildPinCheck(builder);
        break;

    case PageId::ParentalPinWrong:
        BuildNotice(builder, {u"This PIN is incorrect.", u"Please try again."}, PageId::ParentalPinCheck);
        break;

    case PageId::ParentalAnswerCheck:
        BuildAnswerCheck(builder);
        break;

    case PageId::ParentalAnswerWrong:
        BuildNotice(builder, {u"You have entered", u"an incorrect answer."}, PageId::ParentalAnswerCheck);
        break;

    case PageId::ParentalMasterKeyInformation:
        BuildMasterKeyInformation(builder);
        break;

    case PageId::ParentalMasterKeyEntry:
        BuildMasterKeyEntry(builder);
        break;

    case PageId::ParentalMasterKeyWrong:
        BuildNotice(builder, {u"This master key is incorrect."}, PageId::ParentalMasterKeyEntry);
        break;

    default:
        BuildNotice(builder, {u"Parental Controls have", u"been disabled."}, PageId::SecondIndex);
        break;
    }
}

void ParentalPages::BuildPinChoice(PageBuilder& builder)
{
    builder.AddText(RequestBounds, TextRole::Message, {u"Choose a 4-digit PIN."});
    AddField(builder, 0, 1, typedPin, InputKind::Digits, PinLength, u"Choose a 4-digit PIN.");

    builder.AddBackButton([this] { navigator.GoTo(PageAfterTheSecret(PageId::ParentalGuardianNote)); });
    builder.AddBottomRight(u"OK",
        [this]
        {
            if (IsWholePin(typedPin))
                navigator.GoTo(PageId::ParentalPinRepeat);
        });
}

void ParentalPages::BuildPinRepeat(PageBuilder& builder)
{
    builder.AddText(RequestBounds, TextRole::Message, {u"Please enter the PIN again."});
    AddField(builder, 0, 1, typedRepeat, InputKind::Digits, PinLength, u"Please enter the PIN again.");

    builder.AddBackButton([this] { navigator.GoTo(PageId::ParentalPinChoice); });
    builder.AddBottomRight(u"OK", [this] { AcceptRepeatedPin(); });
}

// The question is chosen with the arrows: six questions, from the first to the last, without going round.
void ParentalPages::BuildQuestionChoice(PageBuilder& builder)
{
    builder.AddText(RequestBounds, TextRole::Message, {u"Choose a secret question in case", u"you forget your PIN."});
    builder.AddChoice(0, 1, std::u16string(QuestionText(draft.secretQuestion)), false, nullptr).name = "Question";

    int lastQuestion = static_cast<int>(SecretQuestions.size()) - 1;
    PageButton& previous = builder.AddStepArrow(ButtonLook::ArrowUp, RatingArrowLeft, RatingArrowUpTop, "PreviousQuestion",
        [this]
        {
            --draft.secretQuestion;
            navigator.Refresh();
        });
    previous.isShown = draft.secretQuestion > 0;

    PageButton& next = builder.AddStepArrow(ButtonLook::ArrowDown, RatingArrowLeft, RatingArrowDownTop, "NextQuestion",
        [this]
        {
            ++draft.secretQuestion;
            navigator.Refresh();
        });
    next.isShown = draft.secretQuestion < lastQuestion;

    builder.AddBackButton([this] { navigator.GoTo(PageAfterTheSecret(PageId::ParentalPinChoice)); });
    builder.AddBottomRight(u"OK", [this] { navigator.GoTo(PageId::ParentalAnswerChoice); });
}

void ParentalPages::BuildAnswerChoice(PageBuilder& builder)
{
    builder.AddText(RequestBounds, TextRole::Message, {std::u16string(QuestionText(draft.secretQuestion))});
    builder.AddText({24, 290, 560, 44}, TextRole::Message, {u"Enter the answer to your secret question."});
    AddField(builder, 0, 1, typedAnswer, InputKind::Text, LongestAnswer, QuestionText(draft.secretQuestion));

    builder.AddBackButton([this] { navigator.GoTo(PageId::ParentalQuestionChoice); });
    builder.AddBottomRight(u"OK", [this] { AcceptAnswer(); });
}

// Middle_index of the pages: where the user goes on to the rating, the other restrictions, the end or the clearing of everything.
void ParentalPages::BuildSettingsMenu(PageBuilder& builder)
{
    builder.AddMenuItem(0, u"Game Settings and PIN", [this] { navigator.GoTo(PageId::ParentalSettingsOverview); });
    builder.AddMenuItem(1, u"Other Settings",
        [this]
        {
            shownRestriction = 0;
            navigator.GoTo(PageId::ParentalRestriction);
        });
    builder.AddMenuItem(2, u"Settings Complete", [this] { CompleteSettings(); });
    builder.AddMenuItem(3, u"Clear settings", [this] { navigator.GoTo(PageId::ParentalClearQuestion); });
}

// Re_Setting_index of the pages: each secret and the rating can be changed on its own.
void ParentalPages::BuildSettingsOverview(PageBuilder& builder)
{
    builder.AddMenuItem(0, u"PIN", [this] { navigator.GoTo(PageId::ParentalPinChoice); });
    builder.AddMenuItem(1, u"Secret Question", [this] { navigator.GoTo(PageId::ParentalQuestionChoice); });
    builder.AddMenuItem(2, u"Answer", [this] { navigator.GoTo(PageId::ParentalAnswerChoice); });
    builder.AddMenuItem(3, u"Highest Game Rating Allowed", [this] { navigator.GoTo(PageId::ParentalRatingChoice); });
    builder.AddBottomRight(u"Confirm", [this] { navigator.GoTo(PageId::ParentalSettingsMenu); });
}

// One rating at a time with its picture, as the pages ESRB01 to ESRB06 show them; the arrows go to the stricter or the looser one.
void ParentalPages::BuildRatingChoice(PageBuilder& builder)
{
    const Rating& rating = Ratings[shownRating];
    builder.AddText(RatingNameBounds, TextRole::Message, {std::u16string(rating.name)});
    builder.AddText(RatingMeaningBounds, TextRole::Message, {std::u16string(rating.meaning)});

    bool isWidePicture = shownRating == 0;
    builder.AddPicture(std::string(rating.picture), isWidePicture ? WideRatingPictureBounds : RatingPictureBounds);

    int lastRating = static_cast<int>(Ratings.size()) - 1;
    PageButton& looser = builder.AddStepArrow(ButtonLook::ArrowUp, RatingArrowLeft, RatingArrowUpTop, "LooserRating", [this] { ChangeRating(-1); });
    looser.isShown = shownRating > 0;

    PageButton& stricter = builder.AddStepArrow(ButtonLook::ArrowDown, RatingArrowLeft, RatingArrowDownTop, "StricterRating", [this] { ChangeRating(1); });
    stricter.isShown = shownRating < lastRating;

    builder.AddBottomLeft(u"Cancel", [this] { navigator.GoTo(PageId::ParentalSettingsOverview); }).sound = PageSound::Back;
    builder.AddBottomRight(u"OK",
        [this]
        {
            draft.highestAllowedAge = Ratings[shownRating].age;
            navigator.GoTo(PageId::ParentalSettingsOverview);
        });
}

// "Other Settings": one restriction on each page, with OK going on to the next one and the last OK back to the menu.
void ParentalPages::BuildRestriction(PageBuilder& builder)
{
    builder.SetTitle(2, std::u16string(OtherSettingsTitle));
    builder.AddText(RequestBounds, TextRole::Message, {std::u16string(RestrictionNames[shownRestriction])});

    bool isRestricted = RestrictionFlag(draftRestrictions, shownRestriction);
    builder.AddChoice(0, 2, u"Do not restrict", !isRestricted, [this] { ChooseRestriction(false); });
    builder.AddChoice(1, 2, u"Restrict", isRestricted, [this] { ChooseRestriction(true); });

    builder.AddBackButton(
        [this]
        {
            draftRestrictions = settings.GetParentalRestrictions();
            navigator.GoTo(PageId::ParentalSettingsMenu);
        });
    builder.AddBottomRight(u"OK", [this] { ShowNextRestriction(); });
}

void ParentalPages::BuildPinCheck(PageBuilder& builder)
{
    builder.AddText(RequestBounds, TextRole::Message, {u"Enter your PIN."});
    AddField(builder, 0, 2, typedCheck, InputKind::Digits, PinLength, u"Enter your PIN.");
    builder.AddChoice(1, 2, u"I forgot", false, [this] { navigator.GoTo(PageId::ParentalAnswerCheck); }).sound = PageSound::Decide;

    builder.AddBottomLeft(u"Cancel", [this] { navigator.GoTo(PageId::SecondIndex); }).sound = PageSound::Back;
    builder.AddBottomRight(u"OK", [this] { CheckPin(); });
}

void ParentalPages::BuildAnswerCheck(PageBuilder& builder)
{
    builder.AddText(RequestBounds, TextRole::Message, {std::u16string(QuestionText(draft.secretQuestion))});
    AddField(builder, 0, 2, typedCheck, InputKind::Text, LongestAnswer, QuestionText(draft.secretQuestion));
    builder.AddChoice(1, 2, u"I forgot", false, [this] { navigator.GoTo(PageId::ParentalMasterKeyInformation); }).sound = PageSound::Decide;

    builder.AddBottomLeft(u"Cancel", [this] { navigator.GoTo(PageId::SecondIndex); }).sound = PageSound::Back;
    builder.AddBottomRight(u"OK", [this] { CheckAnswer(); });
}

void ParentalPages::BuildMasterKeyInformation(PageBuilder& builder)
{
    std::u16string confirmationNumber = Settings::ConfirmationNumber(networkConfig.GetWirelessMacAddress());
    BuildNotice(builder,
        {u"If you have forgotten both your PIN", u"and secret question, call the", u"consumer service number listed in",
            u"the Wii Operations Manual and", u"provide the confirmation number", u"below. You will be issued a master key.", u"",
            u"Confirmation Number: " + confirmationNumber},
        PageId::ParentalMasterKeyEntry);
}

void ParentalPages::BuildMasterKeyEntry(PageBuilder& builder)
{
    builder.AddText(RequestBounds, TextRole::Message, {u"Enter the master key."});
    AddField(builder, 0, 1, typedCheck, InputKind::Digits, MasterKeyLength, u"Enter the master key.");

    builder.AddBottomLeft(u"Cancel", [this] { navigator.GoTo(PageId::SecondIndex); }).sound = PageSound::Back;
    builder.AddBottomRight(u"OK", [this] { CheckMasterKey(); });
}

void ParentalPages::BuildNotice(PageBuilder& builder, std::vector<std::u16string> lines, PageId nextPage)
{
    builder.AddMessage(std::move(lines));
    builder.AddBottomRight(u"OK", [this, nextPage] { navigator.GoTo(nextPage); });
}

void ParentalPages::AddField(PageBuilder& builder, int slot, int slotCount, std::u16string& typedText, InputKind kind, std::size_t maximumLength,
    std::u16string_view title)
{
    std::u16string shownText = kind == InputKind::Digits ? StarsFor(typedText) : typedText;

    PageButton& field = builder.AddChoice(slot, slotCount, shownText, false,
        [this, &typedText, kind, maximumLength, title = std::u16string(title)]
        {
            InputRequest request;
            request.kind = kind;
            request.title = title;
            request.maximumLength = maximumLength;

            navigator.RequestInput(request,
                [this, &typedText, maximumLength](std::optional<std::u16string> answer)
                {
                    if (answer)
                        typedText = answer->substr(0, maximumLength);

                    navigator.Refresh();
                });
        });
    field.name = "Field";
    field.sound = PageSound::Decide;
}

void ParentalPages::AcceptRepeatedPin()
{
    if (typedRepeat != typedPin)
    {
        navigator.GoTo(PageId::ParentalPinMismatch);
        return;
    }

    draft.pin = PinFromText(typedPin);
    navigator.GoTo(PageAfterTheSecret(PageId::ParentalQuestionChoice));
}

void ParentalPages::AcceptAnswer()
{
    if (typedAnswer.empty())
        return;

    draft.secretAnswer = typedAnswer;
    navigator.GoTo(PageAfterTheSecret(PageId::ParentalSettingsMenu));
}

void ParentalPages::CheckPin()
{
    if (typedCheck == PinAsText(draft.pin))
        navigator.GoTo(PageId::ParentalSettingsMenu);
    else
        navigator.GoTo(PageId::ParentalPinWrong);
}

void ParentalPages::CheckAnswer()
{
    if (typedCheck == draft.secretAnswer)
        navigator.GoTo(PageId::ParentalSettingsMenu);
    else
        navigator.GoTo(PageId::ParentalAnswerWrong);
}

// The master key of today turns the parental controls off and forgets the PIN, as "Clear settings" does.
void ParentalPages::CheckMasterKey()
{
    Settings::CalendarTime today = clock.Now();
    std::u16string confirmationNumber = Settings::ConfirmationNumber(networkConfig.GetWirelessMacAddress());
    if (typedCheck != Settings::MasterKey(confirmationNumber, today.month, today.day))
    {
        navigator.GoTo(PageId::ParentalMasterKeyWrong);
        return;
    }

    settings.SetParentalControls(Settings::ParentalControls());
    settings.SetParentalRestrictions(Settings::ParentalRestrictions());
    settings.Save();
    navigator.GoTo(PageId::ParentalDisabled);
}

void ParentalPages::ChangeRating(int change)
{
    int lastRating = static_cast<int>(Ratings.size()) - 1;
    shownRating = std::clamp(shownRating + change, 0, lastRating);
    navigator.Refresh();
}

void ParentalPages::ChooseRestriction(bool isRestricted)
{
    RestrictionFlag(draftRestrictions, shownRestriction) = isRestricted;
    navigator.Refresh();
}

void ParentalPages::ShowNextRestriction()
{
    ++shownRestriction;
    if (shownRestriction < RestrictionCount)
    {
        navigator.Refresh();
        return;
    }

    navigator.GoTo(PageId::ParentalSettingsMenu);
}

void ParentalPages::CompleteSettings()
{
    draft.isOn = true;
    settings.SetParentalControls(draft);
    settings.SetParentalRestrictions(draftRestrictions);
    settings.Save();
    navigator.GoTo(PageId::SecondIndex);
}

void ParentalPages::ClearSettings()
{
    settings.SetParentalControls(Settings::ParentalControls());
    settings.SetParentalRestrictions(Settings::ParentalRestrictions());
    settings.Save();
    navigator.GoTo(PageId::ParentalCleared);
}

PageId ParentalPages::PageAfterTheSecret(PageId nextStepOfTheSetup) const
{
    if (isChangingOneSetting)
        return PageId::ParentalSettingsOverview;

    return nextStepOfTheSetup;
}

}
