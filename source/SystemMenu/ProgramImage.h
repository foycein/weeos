#pragma once

#include "SystemMenu/TitleContents.h"

#include "Formats/DolExecutable.h"

#include <cstdint>

namespace SystemMenu
{

// The content that holds the main program of the menu, a DOL file.
constexpr std::uint16_t ProgramContentIndex = 8;

// The main program of the System Menu title. A few resources of the menu are not files but data inside the program, at fixed addresses
// of its memory image: the layout of "Back to Wii Menu" and everything the error screen shows.
class ProgramImage
{
public:
    explicit ProgramImage(TitleContents& titleContents);

    // The bytes from the address to the end of the section of the program that holds it.
    // Throws a MenuError when no section holds the address.
    Formats::ByteView BytesAt(std::uint32_t address) const;

private:
    Formats::DolExecutable executable;
};

}
