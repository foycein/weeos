#pragma once

#include "Ios/SdCard/SdPath.h"
#include "Media/PcmSound.h"

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

// An AAC file plays when the channel accepts it (the first sound track is mp4a, one of the twelve sample rates, one or two
// channels) and Windows can decode it; else the channel says "This AAC file format is not supported" (layout slide_bgm_error).
// "All AAC files" and "Shuffle AAC files" play when one of the files does.
bool IsSongPlayable(PhotoSession& session, const SlideShowSong& song);

// The music of an AAC file of the card, decoded, or nothing when it does not play.
std::optional<Media::PcmSound> DecodeMusicFile(PhotoSession& session, const Ios::SdPath& file);

}
