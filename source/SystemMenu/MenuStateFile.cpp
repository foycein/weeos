#include "SystemMenu/MenuStateFile.h"

#include "SystemMenu/MenuError.h"

#include "Formats/FormatError.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view StateFilePath = "/title/00000001/00000002/data/state.dat";

// The menu starts with the drive state 2. WeeOS has no disc drive, so when the menu is left the drive is empty.
constexpr Formats::Byte DiscStateAtStart = 2;

}

// As in the original: main 0x81379D38 starts cold when the file cannot be read.
std::optional<Formats::StateFile> ReadMenuState(NandFiles& nand)
{
    try
    {
        std::optional<Formats::Bytes> bytes = nand.ReadFile(StateFilePath);
        if (!bytes)
            return std::nullopt;

        return Formats::ReadStateFile(*bytes);
    }
    catch (const MenuError&)
    {
        return std::nullopt;
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
}

void WriteMenuState(NandFiles& nand, const Formats::StateFile& state)
{
    try
    {
        nand.WriteFile(StateFilePath, Formats::WriteStateFile(state));
    }
    catch (const MenuError&)
    {
    }
}

void RecordMenuStart(NandFiles& nand)
{
    Formats::StateFile state{};
    state.type = Formats::StateTypeUnknown;
    state.discState = DiscStateAtStart;
    state.returnTo = Formats::StateReturnToMenu;
    WriteMenuState(nand, state);
}

void RecordMenuLeft(NandFiles& nand, Formats::Byte stateType)
{
    Formats::StateFile state = ReadMenuState(nand).value_or(Formats::StateFile{});
    state.flags = Formats::StateCauseIsRecordedBit | Formats::StateCauseShutDownOrReturned;
    state.type = stateType;
    state.discState = Formats::StateDiscStateNoDisc;
    WriteMenuState(nand, state);
}

}
