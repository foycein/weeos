#include "Channels/MiiChannel/ProfileDialog.h"

#include "Channels/MiiChannel/MiiChannelFiles.h"
#include "Channels/MiiChannel/MiiChannelInput.h"

#include "Mii/MiiColors.h"

#include <array>
#include <format>

namespace Channels
{

namespace
{

constexpr std::string_view LayoutName = "profileNigaoe";
constexpr std::string_view PromptGroup = "frmProfTxNull_00";
constexpr std::string_view PromptText = "textProf_00";
constexpr std::string_view NameGroup = "NameNull_00";
constexpr std::array<std::string_view, 2> Backgrounds = {"bg_00", "bg_01"};
constexpr std::string_view ShownBackground = "bg_00";

constexpr std::string_view ChoiceSound = "SEQ_OK";
constexpr std::string_view CancelSound = "SEQ_CANCEL";

constexpr int MonthCount = 12;
constexpr int DayCount = 31;
constexpr int NoBirthday = 0;
constexpr int FavoriteColorCount = 12;

struct QuestionPanes
{
    ProfileQuestion question;
    std::string_view group;
    std::string_view cancelButton;
    std::uint32_t prompt;
};

constexpr std::array<QuestionPanes, 7> QuestionTable = {{
    {ProfileQuestion::Gender, "genderNull_00", "frmGeCan_00", 300002},
    {ProfileQuestion::HowToMake, "EditNull_00", "frmEdBack_00", 300003},
    {ProfileQuestion::Month, "MonthNull_00", "frmMonthCan_00", 300004},
    {ProfileQuestion::Day, "DayNull_00", "frmDayCan_00", 300004},
    {ProfileQuestion::FavoriteColor, "ColorNull_00", "frmColorCan_00", 300005},
    {ProfileQuestion::Creator, "NameProducer_00", "frmProCan_00", 300006},
    {ProfileQuestion::Mingle, "UraNull_00", "", 0},
}};

const QuestionPanes& PanesOf(ProfileQuestion question)
{
    for (const QuestionPanes& panes : QuestionTable)
    {
        if (panes.question == question)
            return panes;
    }
    return QuestionTable.front();
}

}

ProfileDialog::ProfileDialog(ChannelServices& channelServices, const Layout::LayoutArchive& layouts,
    const Formats::NumberedMessageFile& messages)
    : services(channelServices)
    , texts(messages)
    , layout(std::make_unique<SystemMenu::MenuLayout>(channelServices.display, layouts, std::string(LayoutName)))
{
    FillTextsFromUserData(*layout, messages);
    layout->PaneNamed(NameGroup).isVisible = false;

    for (int color = 0; color < FavoriteColorCount; ++color)
    {
        Formats::Pane& tile = layout->PaneNamed(std::format("frmColor_{:02}", color));
        tile.window->content.vertexColors.fill(Mii::FavoriteColors[color]);
    }
}

void ProfileDialog::Ask(ProfileQuestion question, bool hasBackground)
{
    for (std::string_view background : Backgrounds)
        layout->PaneNamed(background).isVisible = hasBackground && background == ShownBackground;

    ShowGroupOf(question);
    openQuestion = question;
    answer.reset();
}

void ProfileDialog::SetCreatorNames(const std::vector<std::u16string>& names)
{
    creatorNameCount = std::min<std::size_t>(names.size(), CreatorChoiceCount);
    for (int place = 0; place < CreatorChoiceCount; ++place)
    {
        bool isShown = static_cast<std::size_t>(place) < creatorNameCount;
        layout->PaneNamed(std::format("frmProTxNull_{:02}", place)).isVisible = isShown;
        if (isShown)
            layout->SetText(std::format("textPro_{:02}", place), names[place]);
    }
}

bool ProfileDialog::IsOpen() const
{
    return openQuestion.has_value();
}

std::optional<ProfileQuestion> ProfileDialog::OpenQuestion() const
{
    return openQuestion;
}

std::optional<ProfileAnswer> ProfileDialog::TakeAnswer()
{
    std::optional<ProfileAnswer> taken = answer;
    answer.reset();
    return taken;
}

Formats::Vector2 ProfileDialog::PlaceOf(std::string_view paneName) const
{
    return layout->PointOfPaneInPicture(paneName, {0, 0}).value_or(Formats::Vector2{0, 0});
}

void ProfileDialog::Update()
{
    layout->Calculate();
    if (!openQuestion)
        return;

    for (const Choice& choice : ChoicesOf(*openQuestion))
    {
        if (!IsPressed(choice.buttonPane))
            continue;

        services.sounds.Request(ChoiceSound);
        Answer({false, choice.choice});
        return;
    }

    std::string cancelButton = CancelButtonOf(*openQuestion);
    if (!cancelButton.empty() && IsPressed(cancelButton))
    {
        services.sounds.Request(CancelSound);
        Answer({true, 0});
    }
}

void ProfileDialog::Draw()
{
    if (openQuestion)
        layout->Draw();
}

std::string ProfileDialog::ButtonOf(ProfileQuestion question, int choice) const
{
    for (const Choice& known : ChoicesOf(question))
    {
        if (known.choice == choice)
            return known.buttonPane;
    }
    return {};
}

std::string ProfileDialog::CancelButtonOf(ProfileQuestion question) const
{
    return std::string(PanesOf(question).cancelButton);
}

std::vector<ProfileDialog::Choice> ProfileDialog::ChoicesOf(ProfileQuestion question) const
{
    std::vector<Choice> choices;
    switch (question)
    {
    case ProfileQuestion::Gender:
        choices = {{"frmMale_00", 0}, {"frmFemale_00", 1}};
        break;
    case ProfileQuestion::HowToMake:
        choices = {{"frmEdit_00", 0}, {"frmEdit_01", 1}};
        break;
    case ProfileQuestion::Month:
        for (int month = 1; month <= MonthCount; ++month)
            choices.push_back({std::format("frmMonth_{:02}", month - 1), month});
        choices.push_back({std::format("frmMonth_{:02}", MonthCount), NoBirthday});
        break;
    case ProfileQuestion::Day:
        for (int day = 1; day <= DayCount; ++day)
            choices.push_back({std::format("frmDay_{:02}", day - 1), day});
        choices.push_back({std::format("frmDay_{:02}", DayCount), NoBirthday});
        break;
    case ProfileQuestion::FavoriteColor:
        for (int color = 0; color < FavoriteColorCount; ++color)
            choices.push_back({std::format("frmColor_{:02}", color), color});
        break;
    case ProfileQuestion::Creator:
        for (std::size_t place = 0; place < creatorNameCount; ++place)
            choices.push_back({std::format("frmPro_{:02}", place), static_cast<int>(place)});
        choices.push_back({std::format("frmPro_{:02}", NewCreatorChoice), NewCreatorChoice});
        break;
    case ProfileQuestion::Mingle:
        choices = {{"frmUraON_00", 1}, {"frmUraOff_00", 0}};
        break;
    }
    return choices;
}

void ProfileDialog::ShowGroupOf(ProfileQuestion question)
{
    for (const QuestionPanes& panes : QuestionTable)
        layout->PaneNamed(panes.group).isVisible = panes.question == question;

    const QuestionPanes& shown = PanesOf(question);
    layout->PaneNamed(PromptGroup).isVisible = shown.prompt != 0;
    if (shown.prompt != 0)
        layout->SetText(PromptText, MessageText(texts, shown.prompt));
}

void ProfileDialog::Answer(ProfileAnswer givenAnswer)
{
    answer = givenAnswer;
    openQuestion.reset();
}

bool ProfileDialog::IsPressed(const std::string& buttonPane) const
{
    PlayerOneInput input = ReadPlayerOne(services);
    if (!input.pointer || !input.WasPressed(Input::RemoteButtons::A))
        return false;

    return layout->IsPointInPane(buttonPane, *input.pointer);
}

}
