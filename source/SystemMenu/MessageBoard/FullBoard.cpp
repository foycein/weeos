#include "SystemMenu/MessageBoard/FullBoard.h"

#include "SystemMenu/DataManagement/DataResult.h"
#include "SystemMenu/DataManagement/DataActions.h"
#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/DataManagementServices.h"
#include "SystemMenu/DialogManager.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"

#include "Formats/CdbSdCopy.h"
#include "Formats/FormatError.h"
#include "Text/MessageCatalog.h"

#include <format>

namespace SystemMenu
{

namespace
{

// Below a tenth of free room the board is nearly full; moving and erasing go on until half is free (0x8133FA64, 0x8133FA8C).
constexpr std::uint64_t NearlyFullPercentage = 10;
constexpr std::uint64_t RoomAgainPercentage = 50;
constexpr std::uint64_t WholePercentage = 100;

constexpr std::string_view SdCopyFolder = "/private/wii/title/HAEA/";

// The database tries this many numbers for a copy whose name is taken on the card (0x8148DC0C).
constexpr int MostNumbersOfACopy = 1000;

// The messages of ipl_common.bmg, in the order of the steps of 0x8135A694.
constexpr std::uint16_t MovingQuestion = 0x39;
constexpr std::uint16_t MovingProgress = 0x3A;
constexpr std::uint16_t MovedMessage = 0x3B;
constexpr std::uint16_t CardFullNothingMovedMessage = 0x3C;
constexpr std::uint16_t NotMovedMessage = 0x3D;
constexpr std::uint16_t CardLockedMessage = 0x3E;
constexpr std::uint16_t UnusableCardMessage = 0x38;
constexpr std::uint16_t ErasingQuestion = 0x93;
constexpr std::uint16_t ErasingProgress = 0x94;
constexpr std::uint16_t ErasedMessage = 0x95;
constexpr std::uint16_t InsertCardMessage = 0x96;
constexpr std::uint16_t CardProcessFailedMessage = 0x97;
constexpr std::uint16_t MovedAndCardFullMessage = 0x98;
constexpr std::uint16_t YesMessage = 0x141;
constexpr std::uint16_t NoMessage = 0x142;
constexpr std::uint16_t OkMessage = 0x2E;

// The initialization vectors of the copies come from the database's own random numbers (0x81490B80): each byte is bits 16 to 23 of
// the next state. The original starts the state from the time of the console when the first copy is made.
constexpr std::uint32_t RandomMultiplier = 0x5D588B65;
constexpr std::uint32_t RandomIncrement = 0x269EC3;
constexpr int RandomByteShift = 16;

std::uint64_t FreePercentage(MessageStore& store)
{
    return store.FreeBytes() * WholePercentage / BoardVolumeSize;
}

}

bool IsBoardNearlyFull(MessageStore& store)
{
    return FreePercentage(store) < NearlyFullPercentage;
}

bool HasBoardRoomAgain(MessageStore& store)
{
    return FreePercentage(store) >= RoomAgainPercentage;
}

std::string SdCopyPathOf(const RecordPlace& place)
{
    return std::string(SdCopyFolder) + PathOf(place);
}

FullBoardProcedure::FullBoardProcedure(MenuServices& menuServices, MessageStore& messageStore)
    : services(menuServices)
    , store(messageStore)
    , randomState(SecondsSince2000(menuServices.wiiMenu.console.GetLocalTime()))
{
}

bool FullBoardProcedure::IsFinished() const
{
    return step == FullBoardStep::Finished;
}

FullBoardStep FullBoardProcedure::Step() const
{
    return step;
}

void FullBoardProcedure::Run()
{
    switch (step)
    {
    case FullBoardStep::LookingAtTheCard:
        LookAtTheCard();
        break;
    case FullBoardStep::AskingToMove:
        ReadTheAnswerToMove();
        break;
    case FullBoardStep::AskingToErase:
        ReadTheAnswerToErase();
        break;
    case FullBoardStep::AskingToEraseInstead:
        ReadTheAnswerToEraseInstead();
        break;
    case FullBoardStep::Moving:
        MoveTheNextRecord();
        break;
    case FullBoardStep::Erasing:
        EraseTheNextRecord();
        break;
    case FullBoardStep::WaitingForTheWindowToClose:
        WaitForTheWindowToClose();
        break;
    case FullBoardStep::ShowingMessage:
        WaitForTheMessage();
        break;
    case FullBoardStep::Finished:
        break;
    }
}

// With a card in the slot the menu offers to move the old messages to it, without one only to erase them (the step 2 of 0x8135A694).
void FullBoardProcedure::LookAtTheCard()
{
    if (IsCardInTheSlot())
        Ask(MovingQuestion, FullBoardStep::AskingToMove);
    else
        Ask(ErasingQuestion, FullBoardStep::AskingToErase);
}

void FullBoardProcedure::ReadTheAnswerToMove()
{
    std::optional<DialogAnswer> answer = services.dialogs.TakeAnswer();
    if (!answer)
        return;

    if (answer == DialogAnswer::ButtonA)
        StartMoving();
    else
        Ask(ErasingQuestion, FullBoardStep::AskingToEraseInstead);
}

// No without a card: the menu asks for one and looks again.
void FullBoardProcedure::ReadTheAnswerToErase()
{
    std::optional<DialogAnswer> answer = services.dialogs.TakeAnswer();
    if (!answer)
        return;

    if (answer == DialogAnswer::ButtonA)
        StartErasing();
    else
        ShowMessage(InsertCardMessage, FullBoardStep::LookingAtTheCard);
}

void FullBoardProcedure::ReadTheAnswerToEraseInstead()
{
    std::optional<DialogAnswer> answer = services.dialogs.TakeAnswer();
    if (!answer)
        return;

    if (answer == DialogAnswer::ButtonA)
        StartErasing();
    else
        LookAtTheCard();
}

// The card is looked at once more before the work: it may have gone, be unusable or locked (the step 8 of 0x8135A694).
void FullBoardProcedure::StartMoving()
{
    std::optional<std::uint16_t> problem = ProblemOfTheCard();
    if (problem)
    {
        ShowMessage(*problem, FullBoardStep::LookingAtTheCard);
        return;
    }

    records = store.RecordsOldestFirst();
    nextRecord = 0;
    freeBytesAtTheStart = store.FreeBytes();

    // The window shows the turning icon above the bar (0x81347040).
    DialogRequest request;
    request.layout = DialogLayout::Message;
    request.message = services.messages.Find(MovingProgress);
    request.showsWaitIcon = true;
    request.showsProgress = true;
    services.dialogs.Open(request);
    step = FullBoardStep::Moving;
}

// One record in every step: its copy goes onto the card and the record leaves the board (the callback 0x8135B350).
void FullBoardProcedure::MoveTheNextRecord()
{
    if (HasBoardRoomAgain(store) || nextRecord >= records.size())
    {
        FinishMoving(MoveResult::Done);
        return;
    }

    const RecordPlace& place = records[nextRecord];
    try
    {
        CopyToTheCard(place);
    }
    catch (const DataError& error)
    {
        FinishMoving(error.Result() == DataResult::NotEnoughRoom ? MoveResult::CardFull : MoveResult::Failed);
        return;
    }
    catch (const Formats::FormatError&)
    {
        FinishMoving(MoveResult::Failed);
        return;
    }

    store.DeleteMessageWithoutSaving(place);
    ++nextRecord;
    services.dialogs.SetProgress(MovedPercentage());
}

// The bar goes to its end when the work reached it, otherwise the window closes; then the message of how it ended (the steps 10 and
// 11 of 0x8135A694). A full card that still made room enough ends the procedure, one that did not leads back to the question.
void FullBoardProcedure::FinishMoving(MoveResult result)
{
    store.SaveToNand();

    int percentage = MovedPercentage();
    if (percentage < static_cast<int>(WholePercentage))
        services.dialogs.Close();
    else
        services.dialogs.SetProgress(percentage);

    messageAfterTheWindow = NotMovedMessage;
    stepAfterTheMessage = FullBoardStep::LookingAtTheCard;
    if (result == MoveResult::Done)
    {
        messageAfterTheWindow = MovedMessage;
        stepAfterTheMessage = FullBoardStep::Finished;
    }
    else if (result == MoveResult::CardFull && !IsBoardNearlyFull(store))
    {
        messageAfterTheWindow = MovedAndCardFullMessage;
        stepAfterTheMessage = FullBoardStep::Finished;
    }
    else if (result == MoveResult::CardFull)
    {
        messageAfterTheWindow = CardFullNothingMovedMessage;
    }
    step = FullBoardStep::WaitingForTheWindowToClose;
}

void FullBoardProcedure::StartErasing()
{
    records = store.RecordsOldestFirst();
    nextRecord = 0;

    DialogRequest request;
    request.layout = DialogLayout::Message;
    request.message = services.messages.Find(ErasingProgress);
    request.showsWaitIcon = true;
    services.dialogs.Open(request);
    step = FullBoardStep::Erasing;
}

// One record in every step, the oldest first (the callback 0x8135B1E8).
void FullBoardProcedure::EraseTheNextRecord()
{
    if (HasBoardRoomAgain(store) || nextRecord >= records.size())
    {
        store.SaveToNand();
        services.dialogs.Close();
        messageAfterTheWindow = ErasedMessage;
        stepAfterTheMessage = FullBoardStep::Finished;
        step = FullBoardStep::WaitingForTheWindowToClose;
        return;
    }

    store.DeleteMessageWithoutSaving(records[nextRecord]);
    ++nextRecord;
}

void FullBoardProcedure::WaitForTheWindowToClose()
{
    if (!services.dialogs.TakeAnswer())
        return;

    ShowMessage(messageAfterTheWindow, stepAfterTheMessage);
}

void FullBoardProcedure::WaitForTheMessage()
{
    if (!services.dialogs.TakeAnswer())
        return;

    step = stepAfterTheMessage;
    if (step == FullBoardStep::LookingAtTheCard)
        LookAtTheCard();
}

// Yes is A and No is B. The procedure does not turn the sounds of the buttons round, so Yes sounds WIPL_SE_CANCEL as in the original
// (0x81346AD4 with 0, 0x8134753C).
void FullBoardProcedure::Ask(std::uint16_t message, FullBoardStep questionStep)
{
    DialogRequest request;
    request.layout = DialogLayout::TwoButtons;
    request.message = services.messages.Find(message);
    request.buttonAText = services.messages.Find(YesMessage);
    request.buttonBText = services.messages.Find(NoMessage);
    services.dialogs.Open(request);
    step = questionStep;
}

void FullBoardProcedure::ShowMessage(std::uint16_t message, FullBoardStep nextStep)
{
    DialogRequest request;
    request.layout = DialogLayout::OneButton;
    request.message = services.messages.Find(message);
    request.buttonBText = services.messages.Find(OkMessage);
    services.dialogs.Open(request);
    stepAfterTheMessage = nextStep;
    step = FullBoardStep::ShowingMessage;
}

bool FullBoardProcedure::IsCardInTheSlot()
{
    if (services.dataManagement == nullptr)
        return false;

    return services.dataManagement->sdCard.GetStatus().isInserted;
}

std::optional<std::uint16_t> FullBoardProcedure::ProblemOfTheCard()
{
    if (!IsCardInTheSlot())
        return CardProcessFailedMessage;
    if (ReadCardState(*services.dataManagement) == CardState::Unusable)
        return UnusableCardMessage;
    if (services.dataManagement->sdCard.GetStatus().isWriteProtected)
        return CardLockedMessage;

    return std::nullopt;
}

// A copy whose name is taken on the card gets the next number (0x8148DC0C); the key in its header keeps the record's own.
void FullBoardProcedure::CopyToTheCard(const RecordPlace& place)
{
    std::optional<Formats::Bytes> recordFile = store.ReadRecordFile(place);
    if (!recordFile)
        throw DataError(DataResult::Failed, std::format("the record {} is gone", PathOf(place)));

    Formats::Bytes copy = Formats::MakeCdbSdCopy(*recordFile, RecordKeyOf(place), MakeInitializationVector(), store.SdCopyKey());

    Ios::SdCardService& sdCard = services.dataManagement->sdCard;
    RecordPlace placeOnTheCard = place;
    while (SdFileSize(sdCard, MakeSdPath(SdCopyPathOf(placeOnTheCard))))
    {
        ++placeOnTheCard.number;
        if (placeOnTheCard.number >= MostNumbersOfACopy)
            throw DataError(DataResult::Failed, std::format("every name of the copy of {} is taken", PathOf(place)));
    }

    Ios::SdPath copyPath = MakeSdPath(SdCopyPathOf(placeOnTheCard));
    MakeSdFolders(sdCard, copyPath.Parent());
    WriteSdFile(sdCard, copyPath, copy);
}

Formats::AesBlock FullBoardProcedure::MakeInitializationVector()
{
    Formats::AesBlock vector{};
    for (Formats::Byte& byte : vector)
    {
        randomState = randomState * RandomMultiplier + RandomIncrement;
        byte = static_cast<Formats::Byte>(randomState >> RandomByteShift);
    }
    return vector;
}

// How far the work has come: the room it freed against all the room that was taken at the start (0x8135B1C0).
int FullBoardProcedure::MovedPercentage()
{
    std::uint64_t takenAtTheStart = BoardVolumeSize - freeBytesAtTheStart;
    if (takenAtTheStart == 0)
        return static_cast<int>(WholePercentage);

    std::uint64_t freed = store.FreeBytes() - freeBytesAtTheStart;
    return static_cast<int>(freed * WholePercentage / takenAtTheStart);
}

}
