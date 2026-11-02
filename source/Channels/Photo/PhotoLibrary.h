#pragma once

#include "Ios/SdCard/SdCardService.h"

#include "Formats/Bytes.h"

#include <optional>

#include <string>
#include <vector>

namespace Channels
{

enum class PhotoFileKind
{
    Photo,
    QuickTimeMovie,
    AviMovie,
};

// A file of the SD card that the channel shows.
struct PhotoFile
{
    Ios::SdPath path;
    PhotoFileKind kind = PhotoFileKind::Photo;

    // A photo of the Wii Message Board: the JPEG file of a letter's picture, in place of a file of the card.
    std::optional<Formats::Bytes> boardPicture;

    bool IsMovie() const
    {
        return kind != PhotoFileKind::Photo;
    }
};

// What the search of the SD card found (the original's loader 0x80014510):
// photos (.jpg, .jpeg), movies (.mov, .avi) and music (.m4a), in every folder of the card down to ten levels.
struct PhotoLibrary
{
    // At most ShownFileLimit photos and movies, in the order of the search.
    std::vector<PhotoFile> files;

    // Every photo and movie that was found, also those beyond the limit.
    int foundCount = 0;

    std::vector<Ios::SdPath> musicFiles;

    bool HasMoreThanItShows() const
    {
        return foundCount > static_cast<int>(files.size());
    }

    int MovieCount() const;
    int PhotoCount() const;
};

// The original keeps 1000 entries and counts the rest ("Because of limited space, not all files will be displayed.").
constexpr int ShownFileLimit = 1000;

// "Choose a song (Up to 100 files can be listed)".
constexpr int ListedMusicLimit = 100;

// Searches the whole card. Folders deeper than ten levels, files of 2 GB or more and paths longer than the original's buffer
// are left out. MP3 files are left out too: the original names them and skips them.
PhotoLibrary SearchSdCard(Ios::SdCardService& sdCard);

// The kind of a file by its extension, without regard to capital letters. False for a file the channel does not show.
bool PhotoFileKindOfName(std::string_view name, PhotoFileKind& kind);
bool IsMusicFileName(std::string_view name);

}
