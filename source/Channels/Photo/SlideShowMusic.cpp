#include "Channels/Photo/SlideShowMusic.h"

#include "Channels/ChannelText.h"

#include <array>

namespace Channels
{

namespace
{

struct BuiltInSong
{
    std::string_view name;
    std::string_view soundName;
};

// The names are texts of the program; which song of rev_photo.brsar each one plays is Assumed (CHN-33): in the order of the archive.
constexpr std::array<BuiltInSong, 5> BuiltInSongs = {{
    {"Calm", "BGM_SLIDE1"},
    {"Bright", "BGM_SLIDE2"},
    {"Nostalgic", "BGM_SLIDE3"},
    {"Beautiful", "BGM_SLIDE4"},
    {"Scenic", "BGM_SLIDE5"},
}};

constexpr std::string_view NoMusicName = "None";
constexpr std::string_view AllAacFilesName = "All AAC files";
constexpr std::string_view ShuffledAacFilesName = "Shuffle AAC files";

}

std::vector<SlideShowSong> ListSlideShowSongs(const std::vector<Ios::SdPath>& musicFiles)
{
    std::vector<SlideShowSong> songs;
    for (const BuiltInSong& song : BuiltInSongs)
        songs.push_back({SongKind::BuiltIn, AsciiToText(song.name), song.soundName, std::nullopt});

    songs.push_back({SongKind::NoMusic, AsciiToText(NoMusicName), {}, std::nullopt});
    if (musicFiles.empty())
        return songs;

    songs.push_back({SongKind::AllAacFiles, AsciiToText(AllAacFilesName), {}, std::nullopt});
    songs.push_back({SongKind::ShuffledAacFiles, AsciiToText(ShuffledAacFilesName), {}, std::nullopt});
    for (const Ios::SdPath& file : musicFiles)
        songs.push_back({SongKind::AacFile, AsciiToText(file.Name()), {}, file});

    return songs;
}

bool IsSongPlayable(PhotoSession&, const SlideShowSong& song)
{
    return song.kind == SongKind::BuiltIn || song.kind == SongKind::NoMusic;
}

}
