#pragma once

#include "Channels/ChannelServices.h"

#include "SystemMenu/MenuLayout.h"

#include "Formats/NumberedMessageFile.h"
#include "Layout/LayoutArchive.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace Channels
{

// The messages of the Mii Channel by their numbers in nigaoe_message_U.bmg.
namespace MiiMessage
{

constexpr std::uint32_t Ok = 1;
constexpr std::uint32_t Yes = 4;
constexpr std::uint32_t No = 5;
constexpr std::uint32_t PlazaIsFullCannotCreate = 28;
constexpr std::uint32_t FriendsNotRegistered = 12;
constexpr std::uint32_t EraseThisMii = 15;
constexpr std::uint32_t QuitWithoutSaving = 16;
constexpr std::uint32_t ControlsHelp = 23;
constexpr std::uint32_t SaveAndQuit = 25;
constexpr std::uint32_t QuitWithoutSavingButton = 26;
constexpr std::uint32_t Cancel = 27;
constexpr std::uint32_t BrokenDatabase = 35;
constexpr std::uint32_t Welcome = 40;
constexpr std::uint32_t MakeYourOwnMii = 41;
constexpr std::uint32_t ThereIsYourMii = 42;
constexpr std::uint32_t CannotEditOthersMiis = 46;
constexpr std::uint32_t WelcomeToTheParade = 51;
constexpr std::uint32_t ParadeExplained = 52;
constexpr std::uint32_t NobodyIsHere = 53;
constexpr std::uint32_t SendToParadeQuestion = 600200;
constexpr std::uint32_t SpecialMiisCannotJoinParade = 600500;
constexpr std::uint32_t TravelNeedsWiiConnect24 = 70;
constexpr std::uint32_t BeginDsCommunication = 78;
constexpr std::uint32_t CompleteTheProfile = 79;
constexpr std::uint32_t HelpAndHomeButton = 80;
constexpr std::uint32_t NoDsFound = 9;
constexpr std::uint32_t NoSaveDataOnRemote = 87;
constexpr std::uint32_t Formatting = 500703;
constexpr std::uint32_t StartOver = 500701;
constexpr std::uint32_t ReturnToWiiMenu = 500711;

}

enum class MessageAnswer
{
    Yes,
    No,
    Ok,

    // The three buttons of msgBg_04, from the top.
    FirstChoice,
    SecondChoice,
    ThirdChoice,
};

// The message window of the Mii Channel (MsgNigaoe_00): a text over the dimmed picture with Yes and No (msgBg_00) or OK (msgBg_01).
// A press of a button closes it with the answer.
class MessageWindow
{
public:
    // The services, the archive and the messages must outlive the window.
    MessageWindow(ChannelServices& channelServices, const Layout::LayoutArchive& layouts, const Formats::NumberedMessageFile& messages);

    // A question with two buttons, Yes and No unless the numbers say other words (for example Save & quit).
    void AskQuestion(std::uint32_t messageNumber, std::uint32_t yesNumber = MiiMessage::Yes, std::uint32_t noNumber = MiiMessage::No);

    // A message with one button.
    void ShowNotice(std::uint32_t messageNumber, std::uint32_t okNumber = MiiMessage::Ok);

    // Three buttons and no text, as "Quit" of the editor offers them: Save & quit, Quit without saving, Cancel.
    void OfferThreeChoices(std::uint32_t firstNumber, std::uint32_t secondNumber, std::uint32_t thirdNumber);

    bool IsOpen() const;

    // The number of the message that is shown, while the window is open.
    std::optional<std::uint32_t> ShownMessage() const;

    // The answer, once, after a button closed the window.
    std::optional<MessageAnswer> TakeAnswer();

    // Where the middle of a pane is in the picture, in layout units, for the tests that point at it. (0, 0) for a pane that is hidden.
    Formats::Vector2 PlaceOf(std::string_view paneName) const;

    void Update();
    void Draw();

private:
    void Open(int windowNumber, std::uint32_t messageNumber);
    void Close(MessageAnswer answer, std::string_view soundName);
    bool IsPressed(std::string_view buttonPane);

    ChannelServices& services;
    const Formats::NumberedMessageFile& texts;
    std::unique_ptr<SystemMenu::MenuLayout> layout;
    std::optional<std::uint32_t> shownMessage;
    std::optional<MessageAnswer> answer;
    int shownWindow = 0;
};

}
