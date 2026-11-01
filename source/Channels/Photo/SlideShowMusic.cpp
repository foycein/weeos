#include "Channels/Photo/SlideShowMusic.h"

#include "Channels/ChannelText.h"
#include "Channels/Photo/PhotoPictures.h"
#include "Channels/Photo/PhotoSession.h"

#include "Media/AacMusic.h"
#include "Media/QuickTimeReader.h"

#include "Formats/FormatError.h"

#include <algorithm>
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

// The sample rates of AAC files that the channel plays (the table at 0x801D7940).
constexpr std::array<int, 12> AcceptedSampleRates = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};

bool IsAcceptedAacFile(Formats::ByteView file)
{
    Media::Movie music = Media::ReadQuickTimeMovie(file);
    if (!music.sound || music.sound->kind != Media::MovieSoundKind::Aac)
        return false;

    bool hasAcceptedRate = std::find(AcceptedSampleRates.begin(), AcceptedSampleRates.end(), music.sound->sampleRate) != AcceptedSampleRates.end();
    bool hasAcceptedChannels = music.sound->channelCount >= 1 && music.sound->channelCount <= 2;
    return hasAcceptedRate && hasAcceptedChannels;
}
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

std::optional<Media::PcmSound> DecodeMusicFile(PhotoSession& session, const Ios::SdPath& file)
{
    std::optional<Formats::Bytes> bytes = ReadCardFile(session.Services().storage.sdCard, file);
    if (!bytes)
        return std::nullopt;

    try
    {
        if (!IsAcceptedAacFile(*bytes))
            return std::nullopt;

        return Media::DecodeAacMusic(*bytes);
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
}

bool IsSongPlayable(PhotoSession& session, const SlideShowSong& song)
{
    if (song.kind == SongKind::BuiltIn || song.kind == SongKind::NoMusic)
        return true;

    if (song.kind == SongKind::AacFile)
        return DecodeMusicFile(session, *song.file).has_value();

    for (const Ios::SdPath& file : session.library.musicFiles)
    {
        if (DecodeMusicFile(session, file))
            return true;
    }
    return false;
}

}
