#pragma once

#include <stdexcept>
#include <string>

namespace SystemMenu
{

// How a piece of work of Data Management ended. The numbers are those that the worker of the original gives back to its screens
// (iplNandSDWorker.cpp); the screens choose their message by them.
enum class DataResult : int
{
    Done = 0,
    Failed = -2,

    // The file on the SD card is damaged, or is not the file of this title.
    DamagedBackup = -3,

    AlreadyThere = -6,

    // The NAND, the SD card or the slots for channels have no room for it.
    NotEnoughRoom = -7,

    // The SD card is write protected.
    CardLocked = -11,

    // A part of the path on the SD card is a file where a folder should be.
    CardPathBlocked = -12,

    // The save data must not be copied.
    MayNotBeCopied = -13,

    // The title of the save data is not on the NAND: the game was never played on this console.
    TitleNotInstalled = -14,

    // The channel was copied by another console, or every file of the save data may only stay on the console that made it.
    FromAnotherConsole = -15,

    // The NAND holds a newer version of the channel.
    NewerVersionInstalled = -16,
};

// Thrown inside the work of Data Management when a step fails. The public calls catch it and give back the result.
class DataError : public std::runtime_error
{
public:
    DataError(DataResult failure, const std::string& message)
        : std::runtime_error(message)
        , result(failure)
    {
    }

    DataResult Result() const
    {
        return result;
    }

private:
    DataResult result;
};

}
