#pragma once

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
};

// Takes the System Menu, the title that boot2 starts, and reads from the NAND folder its ticket and its title metadata.
// Throws a BootError when the NAND has no ticket or no title metadata for it, when the ticket belongs to another title
// or when the title metadata does not name an IOS.
BootSelection SelectBootTitle(const std::filesystem::path& nandFolder);

}
