#include "SystemMenu/ProgramImage.h"

#include "SystemMenu/MenuError.h"

#include <format>

namespace SystemMenu
{

ProgramImage::ProgramImage(TitleContents& titleContents)
    : executable(Formats::ReadDolExecutable(titleContents.ReadContent(ProgramContentIndex)))
{
}

Formats::ByteView ProgramImage::BytesAt(std::uint32_t address) const
{
    for (const Formats::DolSection& section : executable.sections)
    {
        bool isInSection = address >= section.loadAddress && address - section.loadAddress < section.size;
        if (isInSection)
            return executable.SectionData(section).subspan(address - section.loadAddress);
    }

    throw MenuError(std::format("The main program of the System Menu has nothing at the address {:#010x}", address));
}

}
