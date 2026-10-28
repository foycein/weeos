#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Sound
{

// A name for a place in the commands, so that one file can hold many small sequences.
struct SequenceLabel
{
    std::string name;

    // Counted from the first command.
    std::uint32_t offset;
};

// A sequence file (RSEQ) holds the commands of sequences: what to play, when, and with which instrument.
// A sequence is a list of tracks, and each track runs its own commands. Which sequence to play is told by the offset
// in the sound archive, all offsets count from the first command.
struct SequenceFile
{
    Formats::Bytes commands;
    std::vector<SequenceLabel> labels;

    std::optional<std::uint32_t> FindLabelOffset(std::string_view name) const;
};

SequenceFile ReadSequenceFile(Formats::ByteView bytes);

}
