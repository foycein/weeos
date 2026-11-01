#pragma once

#include "SystemMenu/MenuServices.h"
#include "SystemMenu/MessageBoard/MessageStore.h"

#include "Formats/Aes128.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SystemMenu
{

// True when less than a tenth of the room of cdb.vff is free (0x8133FA64). The Wii Menu asks at its start (0x81390478), the board
// in every step while it is shown (0x81390ADC); then the menu makes room before anything else goes on.
bool IsBoardNearlyFull(MessageStore& store);

// True again when half of the room is free (0x8133FA8C): moving and erasing stop there.
bool HasBoardRoomAgain(MessageStore& store);

// Where the copy of a record lies on the SD card: "/private/wii/title/HAEA/" and the path of the record in cdb.vff (0x8148BB74).
std::string SdCopyPathOf(const RecordPlace& place);

enum class FullBoardStep
{
    LookingAtTheCard,
    AskingToMove,
    AskingToErase,
    AskingToEraseInstead,
    Erasing,
    Moving,
    WaitingForTheWindowToClose,
    ShowingMessage,
    Finished,
};

// How moving the old records to the SD card ended.
enum class MoveResult
{
    Done,
    CardFull,
    Failed,
};

// What the menu does with a nearly full Message Board (the loop 0x81335150 with its steps 0x8135A694). With an SD card in the slot it
// asks to move the oldest records to the card, as encrypted copies (Formats/CdbSdCopy.h) under a progress bar; without a card, or
// when the user says No, it asks to erase them. Either goes on, the oldest record first, until half of the room is free. The
// messages and their order are the original's; an unusable or locked card, a failed copy or a full card lead back to the question.
class FullBoardProcedure
{
public:
    // The services and the store must outlive the procedure.
    FullBoardProcedure(MenuServices& menuServices, MessageStore& messageStore);

    // Once in every step, before the dialogs are updated, until it is finished.
    void Run();
    bool IsFinished() const;
    FullBoardStep Step() const;

private:
    void LookAtTheCard();
    void ReadTheAnswerToMove();
    void ReadTheAnswerToErase();
    void ReadTheAnswerToEraseInstead();
    void StartMoving();
    void MoveTheNextRecord();
    void FinishMoving(MoveResult result);
    void StartErasing();
    void EraseTheNextRecord();
    void WaitForTheWindowToClose();
    void WaitForTheMessage();

    void Ask(std::uint16_t message, FullBoardStep step);
    void ShowMessage(std::uint16_t message, FullBoardStep stepAfterTheMessage);
    std::optional<std::uint16_t> ProblemOfTheCard();
    bool IsCardInTheSlot();
    void CopyToTheCard(const RecordPlace& place);
    Formats::AesBlock MakeInitializationVector();
    int MovedPercentage();

    MenuServices& services;
    MessageStore& store;
    FullBoardStep step = FullBoardStep::LookingAtTheCard;

    std::vector<RecordPlace> records;
    std::size_t nextRecord = 0;
    std::uint64_t freeBytesAtTheStart = 0;

    // The window of the work closes, then this message shows, then the procedure goes on with the step.
    std::uint16_t messageAfterTheWindow = 0;
    FullBoardStep stepAfterTheMessage = FullBoardStep::Finished;

    std::uint32_t randomState = 0;
};

}
