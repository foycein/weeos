#pragma once

#include "Ios/EventQueue.h"
#include "Ios/FileSystem/EntryAttributes.h"
#include "Ios/Result.h"
#include "Ios/SdCard/SdFile.h"
#include "Ios/SdCard/SdPath.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Ios
{

struct SdCardState;

// What the slot holds. This is what the status word of the card driver says (inserted, write protected).
struct SdCardStatus
{
    bool isInserted;
    bool isWriteProtected;

    bool operator==(const SdCardStatus&) const = default;
};

enum class SdCardChange
{
    Inserted,
    Removed,
};

// A file or a directory on the card. The length of a directory is zero.
struct SdEntry
{
    std::string name;
    EntryKind kind;
    std::uint64_t length;
};

// The SD card slot, in place of /dev/sdio/slot0.
//
// The console exposes the card at the command level and the FAT library of the SDK builds the file system on top of it.
// Here the card is a folder on the PC and the service gives its files directly. There are no registers, clock, bus width
// or sectors. The card is put into the slot and taken out again by the platform (InsertCard, RemoveCard);
// the programs see the change in the status and as an event.
//
// All calls are thread safe. Every call that needs the card gives back NoCardInserted while the slot is empty.
// A card that is write protected gives back AccessDenied to every call that would change it.
class SdCardService
{
public:
    SdCardService();

    // The platform puts a card into the slot. The card is the folder on the PC. AlreadyExists when a card is in the slot,
    // NotFound when the folder does not exist.
    Result<void> InsertCard(const std::filesystem::path& cardFolder, bool isWriteProtected);

    // The platform takes the card out. Does nothing when the slot is empty.
    void RemoveCard();

    SdCardStatus GetStatus() const;

    // The card driver tells a program that waits for it when a card is put in or taken out. Here the events are kept in order
    // until a program takes them.
    EventQueue<SdCardChange>& Changes();

    // Opens a file that exists. InvalidArgument for a directory.
    Result<SdFile> Open(const SdPath& path, OpenMode mode);

    // The directory that holds the new entry has to exist. AlreadyExists when the name is taken, also by an entry of the other kind.
    Result<void> CreateFile(const SdPath& path);
    Result<void> CreateDirectory(const SdPath& path);

    // A directory has to be empty (NotEmpty), as on a FAT card. FileInUse when the PC does not let go of the file.
    Result<void> DeleteEntry(const SdPath& path);

    // Moves an entry and gives it a new name. FAT replaces nothing: AlreadyExists when the new path is taken.
    // Only the capital and small letters of a name may change.
    Result<void> RenameEntry(const SdPath& from, const SdPath& to);

    // The entries of a directory, ordered by name. The difference between capital and small letters does not count for the order.
    Result<std::vector<SdEntry>> ListDirectory(const SdPath& path) const;

    Result<SdEntry> GetEntryStatus(const SdPath& path) const;

    // The room left on the card in bytes.
    Result<std::uint64_t> GetFreeBytes() const;

    // Deletes everything on the card, as the format of a FAT card does.
    Result<void> Format();

    // Ends the service: whoever waits for a change stops waiting. The card stays where it is.
    void Close();

private:
    std::shared_ptr<SdCardState> state;
    EventQueue<SdCardChange> changes;
};

}
