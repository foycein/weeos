#include "Ios/SdCard/SdCardService.h"

#include "Ios/SdCard/SdCardState.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <mutex>
#include <system_error>
#include <utility>

namespace Ios
{

namespace
{

constexpr std::uintmax_t BytesPerGibibyte = 1024ull * 1024 * 1024;
constexpr std::uintmax_t StandardCardCapacityLimit = 2 * BytesPerGibibyte;

// What a high capacity card gives back in a slot whose driver cannot start it (docs/audit/OpenPoints/Ios.md, SD2). Assumed.
constexpr IosError UnstartedCardError = IosError::StorageFailure;

// The card is as big as the volume that holds its folder. When the volume cannot be measured the insert reports the folder.
SdCardKind KindOfCardAt(const std::filesystem::path& cardFolder)
{
    std::error_code error;
    std::filesystem::space_info space = std::filesystem::space(cardFolder, error);
    if (error)
        return SdCardKind::Standard;

    if (space.capacity > StandardCardCapacityLimit)
        return SdCardKind::HighCapacity;

    return SdCardKind::Standard;
}

IosError ErrorFromHost(const std::error_code& error)
{
    if (error == std::errc::no_such_file_or_directory)
        return IosError::NotFound;

    if (error == std::errc::file_exists)
        return IosError::AlreadyExists;

    if (error == std::errc::directory_not_empty)
        return IosError::NotEmpty;

    if (error == std::errc::no_space_on_device)
        return IosError::NoFreeClusters;

    if (error == std::errc::permission_denied)
        return IosError::AccessDenied;

    if (error == std::errc::invalid_argument || error == std::errc::not_a_directory || error == std::errc::is_a_directory)
        return IosError::InvalidArgument;

    return IosError::StorageFailure;
}

std::ios::openmode StreamModeFor(OpenMode mode)
{
    if (mode == OpenMode::Read)
        return std::ios::in | std::ios::binary;

    return std::ios::in | std::ios::out | std::ios::binary;
}

std::string NameOnTheCard(const std::filesystem::path& hostName)
{
    std::u8string utf8Name = hostName.filename().u8string();
    return std::string(reinterpret_cast<const char*>(utf8Name.data()), utf8Name.size());
}

char LowerCase(char character)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
}

bool IsCharacterBeforeIgnoringCase(char left, char right)
{
    return LowerCase(left) < LowerCase(right);
}

bool IsEntryBeforeIgnoringCase(const SdEntry& first, const SdEntry& second)
{
    return std::lexicographical_compare(first.name.begin(), first.name.end(), second.name.begin(), second.name.end(),
        IsCharacterBeforeIgnoringCase);
}

// Says why a new entry cannot be made at this place, or gives back a success when it can.
Result<void> CheckNewEntryCanBeMade(const std::filesystem::path& hostPath)
{
    std::error_code error;
    std::filesystem::path parent = hostPath.parent_path();
    if (!std::filesystem::exists(parent, error))
        return IosError::NotFound;

    if (!std::filesystem::is_directory(parent, error))
        return IosError::InvalidArgument;

    if (std::filesystem::exists(hostPath, error))
        return IosError::AlreadyExists;

    return Result<void>();
}

}

SdCardService::SdCardService(const IosProfile& iosProfile)
    : profile(iosProfile)
    , state(std::make_shared<SdCardState>())
{
}

Result<void> SdCardService::CheckCardCanBeUsed() const
{
    if (!state->isInserted)
        return IosError::NoCardInserted;

    bool isCardThatDoesNotStart = state->cardKind == SdCardKind::HighCapacity && !profile.sdSupportsHighCapacityCards;
    if (isCardThatDoesNotStart)
        return UnstartedCardError;

    return Result<void>();
}

Result<void> SdCardService::InsertCard(const std::filesystem::path& cardFolder, bool isWriteProtected)
{
    return InsertCard(cardFolder, isWriteProtected, KindOfCardAt(cardFolder));
}

Result<void> SdCardService::InsertCard(const std::filesystem::path& cardFolder, bool isWriteProtected, SdCardKind kind)
{
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->isInserted)
            return IosError::AlreadyExists;

        std::error_code error;
        if (!std::filesystem::is_directory(cardFolder, error))
            return IosError::NotFound;

        state->isInserted = true;
        state->isWriteProtected = isWriteProtected;
        state->cardKind = kind;
        state->cardFolder = cardFolder;
        ++state->cardNumber;
    }

    changes.Post(SdCardChange::Inserted);
    return Result<void>();
}

void SdCardService::RemoveCard()
{
    bool wasInserted;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        wasInserted = state->isInserted;
        state->isInserted = false;
        state->isWriteProtected = false;
        state->cardKind = SdCardKind::Standard;
        state->cardFolder.clear();
    }

    if (wasInserted)
        changes.Post(SdCardChange::Removed);
}

SdCardStatus SdCardService::GetStatus() const
{
    std::lock_guard<std::mutex> lock(state->mutex);
    return SdCardStatus{state->isInserted, state->isWriteProtected};
}

EventQueue<SdCardChange>& SdCardService::Changes()
{
    return changes;
}

Result<SdFile> SdCardService::Open(const SdPath& path, OpenMode mode)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    Result<void> cardCanBeUsed = CheckCardCanBeUsed();
    if (cardCanBeUsed.Failed())
        return cardCanBeUsed.Error();

    if (mode != OpenMode::Read && state->isWriteProtected)
        return IosError::AccessDenied;

    std::filesystem::path hostPath = path.ToHostPath(state->cardFolder);
    std::error_code error;
    std::filesystem::file_status status = std::filesystem::status(hostPath, error);
    if (!std::filesystem::exists(status))
        return IosError::NotFound;

    if (std::filesystem::is_directory(status))
        return IosError::InvalidArgument;

    std::uintmax_t length = std::filesystem::file_size(hostPath, error);
    if (error)
        return ErrorFromHost(error);

    if (length > MaximumSdFileLength)
        return IosError::InvalidArgument;

    std::fstream stream(hostPath, StreamModeFor(mode));
    if (!stream)
        return IosError::StorageFailure;

    return SdFile(state, state->cardNumber, mode, std::move(stream), static_cast<std::uint32_t>(length));
}

Result<void> SdCardService::CreateFile(const SdPath& path)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    Result<void> cardCanBeUsed = CheckCardCanBeUsed();
    if (cardCanBeUsed.Failed())
        return cardCanBeUsed.Error();

    if (state->isWriteProtected)
        return IosError::AccessDenied;

    if (path.IsRoot())
        return IosError::InvalidArgument;

    std::filesystem::path hostPath = path.ToHostPath(state->cardFolder);
    Result<void> canBeMade = CheckNewEntryCanBeMade(hostPath);
    if (canBeMade.Failed())
        return canBeMade;

    std::ofstream file(hostPath, std::ios::binary);
    if (!file)
        return IosError::StorageFailure;

    return Result<void>();
}

Result<void> SdCardService::CreateDirectory(const SdPath& path)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    Result<void> cardCanBeUsed = CheckCardCanBeUsed();
    if (cardCanBeUsed.Failed())
        return cardCanBeUsed.Error();

    if (state->isWriteProtected)
        return IosError::AccessDenied;

    if (path.IsRoot())
        return IosError::InvalidArgument;

    std::filesystem::path hostPath = path.ToHostPath(state->cardFolder);
    Result<void> canBeMade = CheckNewEntryCanBeMade(hostPath);
    if (canBeMade.Failed())
        return canBeMade;

    std::error_code error;
    std::filesystem::create_directory(hostPath, error);
    if (error)
        return ErrorFromHost(error);

    return Result<void>();
}

Result<void> SdCardService::DeleteEntry(const SdPath& path)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    Result<void> cardCanBeUsed = CheckCardCanBeUsed();
    if (cardCanBeUsed.Failed())
        return cardCanBeUsed.Error();

    if (state->isWriteProtected)
        return IosError::AccessDenied;

    if (path.IsRoot())
        return IosError::InvalidArgument;

    std::filesystem::path hostPath = path.ToHostPath(state->cardFolder);
    std::error_code error;
    std::filesystem::file_status status = std::filesystem::status(hostPath, error);
    if (!std::filesystem::exists(status))
        return IosError::NotFound;

    if (std::filesystem::is_directory(status) && !std::filesystem::is_empty(hostPath, error))
        return IosError::NotEmpty;

    std::filesystem::remove(hostPath, error);
    if (!error)
        return Result<void>();

    IosError cause = ErrorFromHost(error);
    if (cause == IosError::AccessDenied)
        return IosError::FileInUse;

    return cause;
}

Result<void> SdCardService::RenameEntry(const SdPath& from, const SdPath& to)
{
    std::lock_guard<std::mutex> lock(state->mutex);
    Result<void> cardCanBeUsed = CheckCardCanBeUsed();
    if (cardCanBeUsed.Failed())
        return cardCanBeUsed.Error();

    if (state->isWriteProtected)
        return IosError::AccessDenied;

    if (from.IsRoot() || to.IsRoot())
        return IosError::InvalidArgument;

    std::filesystem::path fromHostPath = from.ToHostPath(state->cardFolder);
    std::filesystem::path toHostPath = to.ToHostPath(state->cardFolder);
    std::error_code error;
    if (!std::filesystem::exists(fromHostPath, error))
        return IosError::NotFound;

    if (!std::filesystem::is_directory(toHostPath.parent_path(), error))
        return IosError::NotFound;

    bool toIsTaken = std::filesystem::exists(toHostPath, error);
    bool toIsFrom = toIsTaken && std::filesystem::equivalent(fromHostPath, toHostPath, error);
    if (toIsTaken && !toIsFrom)
        return IosError::AlreadyExists;

    std::filesystem::rename(fromHostPath, toHostPath, error);
    if (error)
        return ErrorFromHost(error);

    return Result<void>();
}

Result<std::vector<SdEntry>> SdCardService::ListDirectory(const SdPath& path) const
{
    std::lock_guard<std::mutex> lock(state->mutex);
    Result<void> cardCanBeUsed = CheckCardCanBeUsed();
    if (cardCanBeUsed.Failed())
        return cardCanBeUsed.Error();

    std::filesystem::path hostPath = path.ToHostPath(state->cardFolder);
    std::error_code error;
    std::filesystem::file_status status = std::filesystem::status(hostPath, error);
    if (!std::filesystem::exists(status))
        return IosError::NotFound;

    if (!std::filesystem::is_directory(status))
        return IosError::InvalidArgument;

    std::vector<SdEntry> entries;
    std::filesystem::directory_iterator iterator(hostPath, error);
    const std::filesystem::directory_iterator endOfDirectory;
    while (!error && iterator != endOfDirectory)
    {
        bool isDirectory = iterator->is_directory(error);
        std::uint64_t length = 0;
        if (!isDirectory)
            length = iterator->file_size(error);

        std::string name = NameOnTheCard(iterator->path());
        EntryKind kind = isDirectory ? EntryKind::Directory : EntryKind::File;
        entries.push_back(SdEntry{name, kind, length});
        iterator.increment(error);
    }
    if (error)
        return ErrorFromHost(error);

    std::sort(entries.begin(), entries.end(), IsEntryBeforeIgnoringCase);
    return entries;
}

Result<SdEntry> SdCardService::GetEntryStatus(const SdPath& path) const
{
    std::lock_guard<std::mutex> lock(state->mutex);
    Result<void> cardCanBeUsed = CheckCardCanBeUsed();
    if (cardCanBeUsed.Failed())
        return cardCanBeUsed.Error();

    std::filesystem::path hostPath = path.ToHostPath(state->cardFolder);
    std::error_code error;
    std::filesystem::file_status status = std::filesystem::status(hostPath, error);
    if (!std::filesystem::exists(status))
        return IosError::NotFound;

    std::string name(path.Name());
    if (std::filesystem::is_directory(status))
        return SdEntry{name, EntryKind::Directory, 0};

    std::uintmax_t length = std::filesystem::file_size(hostPath, error);
    if (error)
        return ErrorFromHost(error);

    return SdEntry{name, EntryKind::File, length};
}

Result<std::uint64_t> SdCardService::GetFreeBytes() const
{
    std::lock_guard<std::mutex> lock(state->mutex);
    Result<void> cardCanBeUsed = CheckCardCanBeUsed();
    if (cardCanBeUsed.Failed())
        return cardCanBeUsed.Error();

    std::error_code error;
    std::filesystem::space_info space = std::filesystem::space(state->cardFolder, error);
    if (error)
        return ErrorFromHost(error);

    return std::uint64_t(space.available);
}

Result<void> SdCardService::Format()
{
    std::lock_guard<std::mutex> lock(state->mutex);
    Result<void> cardCanBeUsed = CheckCardCanBeUsed();
    if (cardCanBeUsed.Failed())
        return cardCanBeUsed.Error();

    if (state->isWriteProtected)
        return IosError::AccessDenied;

    std::error_code listingError;
    std::filesystem::directory_iterator iterator(state->cardFolder, listingError);
    const std::filesystem::directory_iterator endOfDirectory;
    while (!listingError && iterator != endOfDirectory)
    {
        std::error_code removalError;
        std::filesystem::remove_all(iterator->path(), removalError);
        if (removalError)
            return ErrorFromHost(removalError);

        iterator.increment(listingError);
    }

    if (listingError)
        return ErrorFromHost(listingError);

    return Result<void>();
}

void SdCardService::Close()
{
    changes.Close();
}

}
