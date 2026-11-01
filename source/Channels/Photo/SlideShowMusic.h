#pragma once

#include "Ios/SdCard/SdPath.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Channels
{

class PhotoSession;

enum class SongKind
{
    BuiltIn,
    NoMusic,
    AllAacFiles,
    ShuffledAacFiles,
    AacFile,
};

// One entry of the list "Choose a song".
struct SlideShowSong
{
    SongKind kind = SongKind::BuiltIn;
    std::u16string name;

    // The sound of rev_photo.brsar for a built-in song, the file for an AAC file.
    std::string_view soundName;
    std::optional<Ios::SdPath> file;
};

// The built-in songs with their names from the program (0x801D8164 to 0x801D81B8), "None", then "All AAC files" and "Shuffle AAC files"
// when the card holds AAC files, and the files themselves (up to 100).
std::vector<SlideShowSong> ListSlideShowSongs(const std::vector<Ios::SdPath>& musicFiles);

// False for the AAC entries: WeeOS has no AAC decoder yet, so the channel answers as for a file it cannot play
// ("This AAC file format is not supported", layout slide_bgm_error).
bool IsSongPlayable(PhotoSession& session, const SlideShowSong& song);

}
