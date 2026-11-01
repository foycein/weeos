#include "SystemMenu/DataManagement/DataActions.h"

#include "SystemMenu/DataManagement/ChannelBackup.h"
#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/DataLists.h"
#include "SystemMenu/DataManagement/DataRemoval.h"
#include "SystemMenu/DataManagement/SaveBackup.h"

#include "Formats/SdCardBackup.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

// The numbers of the messages in the message file of the menu.
namespace Message
{
constexpr std::uint16_t CopyToCardQuestion = 0x105;
constexpr std::uint16_t CopyToWiiQuestion = 0x110;
constexpr std::uint16_t MoveToCardQuestion = 0x10B;
constexpr std::uint16_t MoveToWiiQuestion = 0x113;
constexpr std::uint16_t EraseQuestion = 0xF7;

constexpr std::uint16_t CopyingToCard = 0x106;
constexpr std::uint16_t CopyingToWii = 0x111;
constexpr std::uint16_t MovingToCard = 0x10C;
constexpr std::uint16_t MovingToWii = 0x114;
constexpr std::uint16_t ErasingOnWii = 0xFB;
constexpr std::uint16_t ErasingOnCard = 0x13A;

constexpr std::uint16_t Copied = 0x107;
constexpr std::uint16_t Moved = 0x10D;
constexpr std::uint16_t Erased = 0xF8;

constexpr std::uint16_t CopiedWithoutNoCopyFiles = 0x12B;
constexpr std::uint16_t CopiedWithNoTransferFiles = 0x12A;
constexpr std::uint16_t MovedWithNoTransferFiles = 0x132;
constexpr std::uint16_t CopiedWithFilesLeftOut = 0x123;
constexpr std::uint16_t MovedWithFilesLeftOut = 0x133;

constexpr std::uint16_t NoCard = 0x121;
constexpr std::uint16_t UnusableCard = 0x38;
constexpr std::uint16_t LockedCard = 0x3E;
constexpr std::uint16_t CannotBeCopied = 0x11C;
constexpr std::uint16_t CannotBeMoved = 0xCD;
constexpr std::uint16_t AlreadyOnCard = 0x127;
constexpr std::uint16_t AlreadyOnWii = 0x128;
constexpr std::uint16_t CardIsFull = 0x116;
constexpr std::uint16_t WiiHasNoRoomForChannel = 0x126;
constexpr std::uint16_t WiiHasNoRoomForSaveData = 0x112;
constexpr std::uint16_t NameIsTaken = 0x129;
constexpr std::uint16_t ChannelCannotBeMovedToThisWii = 0x130;
constexpr std::uint16_t ChannelCannotBeCopiedToThisWii = 0x12C;
constexpr std::uint16_t SaveDataCannotBeCopiedToThisWii = 0x12E;
constexpr std::uint16_t SaveDataCannotBeMovedToThisWii = 0x135;
constexpr std::uint16_t NewerChannelCannotBeMoved = 0x139;
constexpr std::uint16_t NewerChannelCannotBeCopied = 0x138;
constexpr std::uint16_t PlayTheGameBeforeCopying = 0x124;
constexpr std::uint16_t PlayTheGameBeforeMoving = 0x136;

constexpr std::uint16_t MayNotHaveBeenCopied = 0xE3;
constexpr std::uint16_t MayNotHaveBeenErased = 0xE5;
constexpr std::uint16_t MayNotHaveBeenMoved = 0xD7;
}

std::uint16_t FailureMessage(DataAction action)
{
    switch (action)
    {
    case DataAction::Move:
        return Message::MayNotHaveBeenMoved;
    case DataAction::Copy:
        return Message::MayNotHaveBeenCopied;
    case DataAction::Erase:
        return Message::MayNotHaveBeenErased;
    }
    return Message::MayNotHaveBeenCopied;
}

std::uint16_t DoneMessage(DataKind kind, DataPlace place, DataAction action, const DataOutcome& outcome)
{
    if (action == DataAction::Erase)
        return Message::Erased;

    bool isMove = action == DataAction::Move;
    if (kind == DataKind::SaveData && place == DataPlace::Wii)
    {
        if (!isMove && outcome.hasNoCopyFiles)
            return Message::CopiedWithoutNoCopyFiles;

        if (outcome.hasNoTransferFiles)
            return isMove ? Message::MovedWithNoTransferFiles : Message::CopiedWithNoTransferFiles;
    }

    if (kind == DataKind::SaveData && outcome.hasLeftOutNoTransferFiles)
        return isMove ? Message::MovedWithFilesLeftOut : Message::CopiedWithFilesLeftOut;

    return isMove ? Message::Moved : Message::Copied;
}

bool IsOnCard(const DataManagementServices& services, DataKind kind, std::uint32_t lowPart)
{
    std::optional<std::uint64_t> size = kind == DataKind::Channels ? CardChannelBytes(services, lowPart) : CardSaveDataBytes(services, lowPart);
    std::size_t headerSize = kind == DataKind::Channels ? Formats::ChannelBackupHeaderSize : Formats::SaveBackupHeaderSize;
    return size && *size > headerSize;
}

bool IsOnWii(DataManagementServices& services, DataKind kind, Ios::TitleId titleId)
{
    if (kind == DataKind::Channels)
        return IsChannelComplete(services.titles, titleId);

    std::vector<Ios::TitleId> saves = ListNandSaveData(services);
    return std::find(saves.begin(), saves.end(), titleId) != saves.end();
}

std::optional<std::uint16_t> CardRefusal(const DataManagementServices& services, bool needsToWrite)
{
    Ios::SdCardStatus status = services.sdCard.GetStatus();
    switch (ReadCardState(services))
    {
    case CardState::NoCard:
        return Message::NoCard;
    case CardState::Unusable:
        return Message::UnusableCard;
    case CardState::Ready:
        break;
    }

    if (needsToWrite && status.isWriteProtected)
        return Message::LockedCard;

    return std::nullopt;
}

}

CardState ReadCardState(const DataManagementServices& services)
{
    if (!services.sdCard.GetStatus().isInserted)
        return CardState::NoCard;

    if (services.sdCard.GetFreeBytes().Failed())
        return CardState::Unusable;

    return CardState::Ready;
}

std::optional<std::uint16_t> RefusalMessage(DataManagementServices& services, DataKind kind, DataPlace place, DataAction action, const DataItem& item)
{
    bool isErase = action == DataAction::Erase;
    bool cannotLeave = item.isDamaged || item.mayNotBeCopied;
    if (!isErase && cannotLeave)
        return action == DataAction::Move ? Message::CannotBeMoved : Message::CannotBeCopied;

    bool writesTheCard = place == DataPlace::SdCard || !isErase;
    if (writesTheCard)
    {
        std::optional<std::uint16_t> cardRefusal = CardRefusal(services, true);
        if (cardRefusal)
            return cardRefusal;
    }

    if (isErase)
        return std::nullopt;

    if (place == DataPlace::Wii && IsOnCard(services, kind, item.lowPart))
        return Message::AlreadyOnCard;

    if (place == DataPlace::SdCard && item.titleId && IsOnWii(services, kind, *item.titleId))
        return Message::AlreadyOnWii;

    return std::nullopt;
}

std::uint16_t QuestionMessage(DataPlace place, DataAction action)
{
    bool fromWii = place == DataPlace::Wii;
    switch (action)
    {
    case DataAction::Move:
        return fromWii ? Message::MoveToCardQuestion : Message::MoveToWiiQuestion;
    case DataAction::Copy:
        return fromWii ? Message::CopyToCardQuestion : Message::CopyToWiiQuestion;
    case DataAction::Erase:
        return Message::EraseQuestion;
    }
    return Message::EraseQuestion;
}

std::uint16_t ProgressMessage(DataPlace place, DataAction action)
{
    bool fromWii = place == DataPlace::Wii;
    switch (action)
    {
    case DataAction::Move:
        return fromWii ? Message::MovingToCard : Message::MovingToWii;
    case DataAction::Copy:
        return fromWii ? Message::CopyingToCard : Message::CopyingToWii;
    case DataAction::Erase:
        return fromWii ? Message::ErasingOnWii : Message::ErasingOnCard;
    }
    return Message::ErasingOnWii;
}

// The table of the results of the original (channels 0x813A2F64, save data 0x813C64F0).
std::uint16_t OutcomeMessage(DataKind kind, DataPlace place, DataAction action, const DataOutcome& outcome)
{
    bool fromWii = place == DataPlace::Wii;
    bool isMove = action == DataAction::Move;
    bool isChannel = kind == DataKind::Channels;

    switch (outcome.result)
    {
    case DataResult::Done:
        return DoneMessage(kind, place, action, outcome);
    case DataResult::AlreadyThere:
        return fromWii ? Message::AlreadyOnCard : Message::AlreadyOnWii;
    case DataResult::NotEnoughRoom:
        if (fromWii)
            return Message::CardIsFull;
        return isChannel ? Message::WiiHasNoRoomForChannel : Message::WiiHasNoRoomForSaveData;
    case DataResult::CardPathBlocked:
        return Message::NameIsTaken;
    case DataResult::CardLocked:
        return Message::LockedCard;
    case DataResult::MayNotBeCopied:
        return Message::CannotBeCopied;
    case DataResult::TitleNotInstalled:
        return isMove ? Message::PlayTheGameBeforeMoving : Message::PlayTheGameBeforeCopying;
    case DataResult::FromAnotherConsole:
        if (isChannel)
            return isMove ? Message::ChannelCannotBeMovedToThisWii : Message::ChannelCannotBeCopiedToThisWii;
        return isMove ? Message::SaveDataCannotBeMovedToThisWii : Message::SaveDataCannotBeCopiedToThisWii;
    case DataResult::NewerVersionInstalled:
        return isMove ? Message::NewerChannelCannotBeMoved : Message::NewerChannelCannotBeCopied;
    default:
        return FailureMessage(action);
    }
}

DataOutcome RunDataAction(DataManagementServices& services, DataKind kind, DataPlace place, DataAction action, const DataItem& item)
{
    DataOutcome outcome;
    bool fromWii = place == DataPlace::Wii;
    if (fromWii && !item.titleId)
    {
        outcome.result = DataResult::Failed;
        return outcome;
    }

    if (kind == DataKind::Channels)
    {
        if (action == DataAction::Move)
            outcome.result = fromWii ? MoveChannelToSdCard(services, *item.titleId) : MoveChannelToNand(services, item.lowPart);
        else if (action == DataAction::Copy)
            outcome.result = fromWii ? CopyChannelToSdCard(services, *item.titleId) : CopyChannelToNand(services, item.lowPart);
        else
            outcome.result = fromWii ? EraseChannelFromNand(services, *item.titleId) : EraseChannelFromCard(services, item.lowPart);
        return outcome;
    }

    if (action == DataAction::Erase)
    {
        outcome.result = fromWii ? EraseSaveDataFromNand(services, *item.titleId) : EraseSaveDataFromCard(services, item.lowPart);
        return outcome;
    }

    bool isMove = action == DataAction::Move;
    if (fromWii)
    {
        SaveCopyReport report = isMove ? MoveSaveDataToSdCard(services, *item.titleId) : CopySaveDataToSdCard(services, *item.titleId);
        outcome.result = report.result;
        outcome.hasNoCopyFiles = report.hasNoCopyFiles;
        outcome.hasNoTransferFiles = report.hasNoTransferFiles;
        return outcome;
    }

    SaveRestoreReport report = isMove ? MoveSaveDataToNand(services, item.lowPart) : CopySaveDataToNand(services, item.lowPart);
    outcome.result = report.result;
    outcome.hasLeftOutNoTransferFiles = report.hasLeftOutNoTransferFiles;
    return outcome;
}

}
