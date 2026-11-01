#include "Channels/MiiChannel/MiiChannelData.h"

#include "SystemMenu/IosNandFiles.h"
#include "SystemMenu/MenuError.h"

#include "Formats/FormatError.h"

#include <array>
#include <format>

namespace Channels
{

namespace
{

// RFL makes its folders readable and writable by everybody (permissions 0x3F, docs/research/Mii.md 2.3).
constexpr std::array<std::string_view, 2> DatabaseFolders = {"/shared2/menu", "/shared2/menu/FaceLib"};
constexpr Ios::EntryPermissions EverybodyReadsAndWrites = {
    Ios::AccessRights::ReadWrite, Ios::AccessRights::ReadWrite, Ios::AccessRights::ReadWrite};
constexpr std::uint8_t NoAttributeFlags = 0;

// The format of the channel clears its record of 0x60 bytes (8001A618, CHN-21 and CHN-72); WeeOS keeps only the flags word in it.
constexpr std::size_t MiiChannelSaveSize = 0x60;
constexpr std::size_t FlagsOffset = 0x40;
constexpr std::size_t FlagsSize = 4;
constexpr int BitsPerByte = 8;

}

MiiChannelData::MiiChannelData(Ios::FileSystemService& fileSystemService, Ios::Caller caller, std::uint32_t consoleSystemId)
    : fileSystem(fileSystemService)
    , fileSystemCaller(caller)
    , files(std::make_unique<SystemMenu::IosNandFiles>(fileSystemService, caller))
    , systemId(consoleSystemId)
    , database(Mii::MiiDatabase::MakeEmpty())
{
    std::optional<Formats::Bytes> save = files->ReadFile(MiiChannelSavePath);
    isFirstStart = !save.has_value();
    channelSave = save.value_or(Formats::Bytes(MiiChannelSaveSize, 0));
    if (channelSave.size() < MiiChannelSaveSize)
        channelSave.resize(MiiChannelSaveSize, 0);

    std::optional<Formats::Bytes> stored = files->ReadFile(MiiDatabasePath);
    if (!stored)
    {
        Format();
        return;
    }

    try
    {
        database = Mii::MiiDatabase::Read(*stored);
    }
    catch (const Formats::FormatError&)
    {
        state = DatabaseState::Broken;
        return;
    }

    // The start-up repair of the Mii Channel (80137BCC): broken and temporary Miis go, and the database is written when one went.
    if (database.RemoveBrokenMiis() > 0)
        WriteDatabase();
}

DatabaseState MiiChannelData::State() const
{
    return state;
}

void MiiChannelData::Format()
{
    database = Mii::MiiDatabase::MakeEmpty();
    CreateDatabaseFolders();
    WriteDatabase();
    state = DatabaseState::Ready;
}

std::uint32_t MiiChannelData::ConsoleSystemId() const
{
    return systemId;
}

std::vector<StoredMii> MiiChannelData::Miis() const
{
    std::vector<StoredMii> miis;
    for (std::size_t slot = 0; slot < Mii::MiiSlotCount; ++slot)
    {
        std::optional<Mii::MiiRecord> record = database.FindMii(slot);
        if (record)
            miis.push_back({slot, *record});
    }
    return miis;
}

std::optional<Mii::MiiRecord> MiiChannelData::FindMii(std::size_t slot) const
{
    return database.FindMii(slot);
}

bool MiiChannelData::IsFull() const
{
    return !database.FirstFreeSlot().has_value();
}

std::optional<std::size_t> MiiChannelData::AddMii(const Mii::MiiRecord& record)
{
    std::optional<std::size_t> slot = database.FirstFreeSlot();
    if (!slot)
        return std::nullopt;

    ReplaceMii(*slot, record);
    return slot;
}

void MiiChannelData::ReplaceMii(std::size_t slot, const Mii::MiiRecord& record)
{
    database.StoreMii(slot, record, systemId);
    WriteDatabase();
}

void MiiChannelData::DeleteMii(std::size_t slot)
{
    database.DeleteMii(slot);
    WriteDatabase();
}

std::vector<Mii::MiiRecord> MiiChannelData::ParadeMiis() const
{
    return database.HiddenMiis();
}

void MiiChannelData::SendToParade(std::size_t slot)
{
    std::optional<Mii::MiiRecord> record = database.FindMii(slot);
    if (!record)
        return;

    database.AddHiddenMii(*record);
    database.DeleteMii(slot);
    WriteDatabase();
}

std::optional<std::size_t> MiiChannelData::BringBackFromParade(std::size_t place)
{
    std::vector<Mii::MiiRecord> paradeMiis = database.HiddenMiis();
    std::optional<std::size_t> slot = database.FirstFreeSlot();
    if (place >= paradeMiis.size() || !slot)
        return std::nullopt;

    database.StoreMii(*slot, paradeMiis[place], systemId);
    WriteDatabase();
    return slot;
}

void MiiChannelData::RemoveFromParade(std::size_t place)
{
    database.RemoveHiddenMii(place);
    WriteDatabase();
}

bool MiiChannelData::IsFirstStart() const
{
    return isFirstStart;
}

void MiiChannelData::RememberStarted()
{
    channelSave.assign(MiiChannelSaveSize, 0);
    files->WriteFile(MiiChannelSavePath, channelSave);
    isFirstStart = false;
}

std::uint32_t MiiChannelData::Flags() const
{
    std::uint32_t flags = 0;
    for (std::size_t byte = 0; byte < FlagsSize; ++byte)
        flags = (flags << BitsPerByte) | channelSave[FlagsOffset + byte];
    return flags;
}

bool MiiChannelData::HasFlag(ChannelFlag flag) const
{
    return (Flags() & static_cast<std::uint32_t>(flag)) != 0;
}

void MiiChannelData::SetFlag(ChannelFlag flag, bool isSet)
{
    std::uint32_t flags = Flags();
    if (isSet)
        flags |= static_cast<std::uint32_t>(flag);
    else
        flags &= ~static_cast<std::uint32_t>(flag);

    for (std::size_t byte = 0; byte < FlagsSize; ++byte)
    {
        int shift = static_cast<int>(FlagsSize - 1 - byte) * BitsPerByte;
        channelSave[FlagsOffset + byte] = static_cast<Formats::Byte>(flags >> shift);
    }
    files->WriteFile(MiiChannelSavePath, channelSave);
}

void MiiChannelData::CreateDatabaseFolders()
{
    for (std::string_view folder : DatabaseFolders)
    {
        Ios::Result<Ios::NandPath> path = Ios::NandPath::Parse(folder);
        Ios::Result<void> created = fileSystem.CreateDirectory(path.Value(), EverybodyReadsAndWrites, NoAttributeFlags, fileSystemCaller);
        if (created.Failed() && created.Error() != Ios::IosError::AlreadyExists)
            throw SystemMenu::MenuError(std::format("The Mii Channel cannot make the folder {}", folder));
    }
}

void MiiChannelData::WriteDatabase()
{
    files->WriteFile(MiiDatabasePath, database.FileBytes());
}

}
