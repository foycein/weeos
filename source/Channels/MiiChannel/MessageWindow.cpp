#include "Channels/MiiChannel/MessageWindow.h"

#include "Channels/MiiChannel/MiiChannelFiles.h"
#include "Channels/MiiChannel/MiiChannelInput.h"

#include <array>
#include <format>

namespace Channels
{

namespace
{

constexpr std::string_view LayoutName = "MsgNigaoe_00";
constexpr int QuestionWindow = 0;
constexpr int NoticeWindow = 1;
constexpr int ThreeChoicesWindow = 4;
constexpr std::array<int, 5> AllWindows = {0, 1, 2, 3, 4};
constexpr std::uint8_t Shown = 255;
constexpr std::uint8_t Hidden = 0;

constexpr std::string_view YesButton = "frmMsgYes_00";
constexpr std::string_view NoButton = "frmMsgNo_00";
constexpr std::string_view OkButton = "frmMsgOk_01";
constexpr std::array<std::string_view, 3> ChoiceButtons = {"frm1Msg_04", "frm2Msg_04", "frm3Msg_04"};
constexpr std::array<MessageAnswer, 3> ChoiceAnswers = {MessageAnswer::FirstChoice, MessageAnswer::SecondChoice, MessageAnswer::ThirdChoice};

constexpr std::string_view YesSound = "SEQ_YES";
constexpr std::string_view NoSound = "SEQ_NO";
constexpr std::string_view OkSound = "SEQ_OK";
constexpr std::string_view OpenSound = "SEQ_WINDOW_OPEN";

}

MessageWindow::MessageWindow(ChannelServices& channelServices, const Layout::LayoutArchive& layouts,
    const Formats::NumberedMessageFile& messages)
    : services(channelServices)
    , texts(messages)
    , layout(std::make_unique<SystemMenu::MenuLayout>(channelServices.display, layouts, std::string(LayoutName)))
{
    FillTextsFromUserData(*layout, messages);
}

void MessageWindow::AskQuestion(std::uint32_t messageNumber, std::uint32_t yesNumber, std::uint32_t noNumber)
{
    Open(QuestionWindow, messageNumber);
    layout->SetText("textMsg_00", MessageText(texts, messageNumber));
    layout->SetText("frmMsgYesTx_00", MessageText(texts, yesNumber));
    layout->SetText("frmMsgNoTx_00", MessageText(texts, noNumber));
}

void MessageWindow::ShowNotice(std::uint32_t messageNumber, std::uint32_t okNumber)
{
    Open(NoticeWindow, messageNumber);
    layout->SetText("textMsg_01", MessageText(texts, messageNumber));
    layout->SetText("frmMsgOkTx_01", MessageText(texts, okNumber));
}

void MessageWindow::OfferThreeChoices(std::uint32_t firstNumber, std::uint32_t secondNumber, std::uint32_t thirdNumber)
{
    Open(ThreeChoicesWindow, firstNumber);
    layout->SetText("text1Msg_04", MessageText(texts, firstNumber));
    layout->SetText("text2Msg_04", MessageText(texts, secondNumber));
    layout->SetText("text3Msg_04", MessageText(texts, thirdNumber));
}

bool MessageWindow::IsOpen() const
{
    return shownMessage.has_value();
}

std::optional<std::uint32_t> MessageWindow::ShownMessage() const
{
    return shownMessage;
}

std::optional<MessageAnswer> MessageWindow::TakeAnswer()
{
    std::optional<MessageAnswer> taken = answer;
    answer.reset();
    return taken;
}

Formats::Vector2 MessageWindow::PlaceOf(std::string_view paneName) const
{
    return layout->PointOfPaneInPicture(paneName, {0, 0}).value_or(Formats::Vector2{0, 0});
}

void MessageWindow::Update()
{
    layout->Calculate();
    if (!IsOpen())
        return;

    if (shownWindow == QuestionWindow)
    {
        if (IsPressed(YesButton))
            Close(MessageAnswer::Yes, YesSound);
        else if (IsPressed(NoButton))
            Close(MessageAnswer::No, NoSound);
        return;
    }

    if (shownWindow == ThreeChoicesWindow)
    {
        for (std::size_t choice = 0; choice < ChoiceButtons.size(); ++choice)
        {
            if (IsPressed(ChoiceButtons[choice]))
            {
                Close(ChoiceAnswers[choice], OkSound);
                return;
            }
        }
        return;
    }

    if (IsPressed(OkButton))
        Close(MessageAnswer::Ok, OkSound);
}

void MessageWindow::Draw()
{
    if (IsOpen())
        layout->Draw();
}

void MessageWindow::Open(int windowNumber, std::uint32_t messageNumber)
{
    for (int window : AllWindows)
    {
        std::uint8_t alpha = window == windowNumber ? Shown : Hidden;
        layout->PaneNamed(std::format("msgBg_{:02}", window)).alpha = alpha;
        layout->PaneNamed(std::format("msgNull_{:02}", window)).alpha = alpha;
    }

    shownWindow = windowNumber;
    shownMessage = messageNumber;
    answer.reset();
    services.sounds.Request(OpenSound);
}

void MessageWindow::Close(MessageAnswer givenAnswer, std::string_view soundName)
{
    services.sounds.Request(soundName);
    shownMessage.reset();
    answer = givenAnswer;
}

bool MessageWindow::IsPressed(std::string_view buttonPane)
{
    PlayerOneInput input = ReadPlayerOne(services);
    if (!input.pointer || !input.WasPressed(Input::RemoteButtons::A))
        return false;

    return layout->IsPointInPane(buttonPane, *input.pointer);
}

}
