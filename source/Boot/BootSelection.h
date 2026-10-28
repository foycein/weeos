#pragma once

#include "Ios/Title/TicketView.h"
#include "Ios/Title/TitleService.h"

#include <cstdint>
#include <filesystem>

namespace Boot
{

// What boot2 decides before anything starts: which title to run, and which IOS that title needs.
struct BootSelection
{
    std::uint64_t titleId;
    std::uint16_t titleVersion;

    // The IOS the title runs on, named by the "system version" in the title metadata.
    std::uint64_t iosTitleId;

    // The first ticket of the title. boot2 starts the title with it.
    Ios::TicketView ticketView;
};

// Takes the System Menu, the title that boot2 starts, and reads from the NAND folder its ticket and its title metadata.
// Throws a BootError when the NAND has no ticket or no title metadata for it, when the ticket belongs to another title
// or when the title metadata does not name an IOS. The files of the folder are read directly, so no IOS needs to run.
BootSelection SelectBootTitle(const std::filesystem::path& nandFolder);

// The same decision, made with the title service of the IOS that runs. This is what the console start uses.
BootSelection SelectBootTitle(Ios::TitleService& titles);

}
