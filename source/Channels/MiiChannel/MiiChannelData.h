#pragma once

#include "SystemMenu/NandFiles.h"

#include "Ios/FileSystem/FileSystemService.h"
#include "Mii/MiiDatabase.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace Channels
{

// Where the Miis of the console live (docs/research/Mii.md 2.3), and the Mii Channel's own file in its data folder.
constexpr std::string_view MiiDatabasePath = "/shared2/menu/FaceLib/RFL_DB.dat";
constexpr std::string_view MiiChannelSavePath = "/title/00010002/48414341/data/NigaoeCh.dat";

// A Mii of the plaza: its slot in the database and the Mii itself.
struct StoredMii
{
    std::size_t slot = 0;
    Mii::MiiRecord record;
};

// What the start of the channel found.
enum class DatabaseState
{
    Ready,

    // The checksum is wrong or the file too short: the face library refuses it until it is formatted (message 35 offers that).
    Broken,
};

// The Miis of the console as the Mii Channel keeps them: RFL_DB.dat read at the start, every change written back at once through the file
// system of IOS, as RFL does (8013EBF4: store and write the main part; 8013EDD8: delete). A missing database is made at the start, as the
// Mii Channel formats one when the file is not there (80010008).
class MiiChannelData
{
public:
    // The services must outlive the data. Throws a MenuError when the NAND cannot be read or written.
    MiiChannelData(Ios::FileSystemService& fileSystem, Ios::Caller caller, std::uint32_t consoleSystemId);

    DatabaseState State() const;

    // Message 35, "Yes": a new, empty database replaces the broken one.
    void Format();

    std::uint32_t ConsoleSystemId() const;

    // The usable Miis of the database, in the order of their slots.
    std::vector<StoredMii> Miis() const;
    std::optional<Mii::MiiRecord> FindMii(std::size_t slot) const;
    bool IsFull() const;

    // A new Mii goes into the first free slot. Gives the slot, or nothing when the plaza is full (100 Miis).
    std::optional<std::size_t> AddMii(const Mii::MiiRecord& record);

    void ReplaceMii(std::size_t slot, const Mii::MiiRecord& record);
    void DeleteMii(std::size_t slot);

    // The Miis of the Mii Parade, the hidden Miis of the database, oldest first. Today they come only from this plaza ("Send to Mii
    // Parade"); the Miis of other consoles will come into the same list (Mii::MiiDatabase::AddHiddenMii).
    std::vector<Mii::MiiRecord> ParadeMiis() const;

    // "Send to Mii Parade": the Mii leaves its slot and joins the parade without its birthday and creator.
    void SendToParade(std::size_t slot);

    // A Mii of the parade (by its place in ParadeMiis) comes back to the plaza, into the first free slot, and leaves the parade.
    // Gives the slot, or nothing when the plaza is full.
    std::optional<std::size_t> BringBackFromParade(std::size_t place);

    void RemoveFromParade(std::size_t place);

    // The first start of the channel: there is no NigaoeCh.dat yet. The welcome then takes the user to the first Mii.
    bool IsFirstStart() const;

    // Writes NigaoeCh.dat, so that the next start is not the first.
    void RememberStarted();

private:
    void CreateDatabaseFolders();
    void WriteDatabase();

    Ios::FileSystemService& fileSystem;
    Ios::Caller fileSystemCaller;
    std::unique_ptr<SystemMenu::NandFiles> files;
    std::uint32_t systemId;
    Mii::MiiDatabase database;
    DatabaseState state = DatabaseState::Ready;
    bool isFirstStart = false;
};

}
