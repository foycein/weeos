#include "Channels/Photo/PhotoLibrary.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace Channels
{

namespace
{

// The original goes ten folders deep (the root is level 0) and refuses paths that do not fit its buffer of 0x101 characters
// with room for the name of the drive and its extension (0x80014510).
constexpr int DeepestFolderLevel = 9;
constexpr std::size_t PathBufferSize = 0x101;
constexpr std::size_t RoomForDriveAndExtension = 0xE;
constexpr std::size_t LongestPath = PathBufferSize - RoomForDriveAndExtension;

// A file of 2 GB or more has the top bit of its FAT size set; the original leaves it out.
constexpr std::uint64_t LargestFileLength = 0x7FFFFFFF;

std::string LowerCaseExtension(std::string_view name)
{
    std::size_t dot = name.rfind('.');
    if (dot == std::string_view::npos)
        return {};

    std::string extension(name.substr(dot + 1));
    for (char& character : extension)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

    return extension;
}

class CardSearch
{
public:
    explicit CardSearch(Ios::SdCardService& card)
        : sdCard(card)
    {
    }

    PhotoLibrary Search()
    {
        SearchFolder(Ios::SdPath::Root(), 0);
        return library;
    }

private:
    void SearchFolder(const Ios::SdPath& folder, int level)
    {
        Ios::Result<std::vector<Ios::SdEntry>> entries = sdCard.ListDirectory(folder);
        if (entries.Failed())
            return;

        for (const Ios::SdEntry& entry : entries.Value())
        {
            std::string pathText = folder.IsRoot() ? "/" + entry.name : folder.ToString() + "/" + entry.name;
            if (pathText.size() > LongestPath)
                continue;

            Ios::Result<Ios::SdPath> path = Ios::SdPath::Parse(pathText);
            if (path.Failed())
                continue;

            if (entry.kind == Ios::EntryKind::Directory)
            {
                if (level < DeepestFolderLevel)
                    SearchFolder(path.Value(), level + 1);
                continue;
            }

            if (entry.length <= LargestFileLength)
                AddFile(path.Value(), entry.name);
        }
    }

    void AddFile(const Ios::SdPath& path, std::string_view name)
    {
        if (IsMusicFileName(name))
        {
            if (library.musicFiles.size() < ListedMusicLimit)
                library.musicFiles.push_back(path);
            return;
        }

        PhotoFileKind kind = PhotoFileKind::Photo;
        if (!PhotoFileKindOfName(name, kind))
            return;

        ++library.foundCount;
        if (library.files.size() < ShownFileLimit)
            library.files.push_back({path, kind});
    }

    Ios::SdCardService& sdCard;
    PhotoLibrary library;
};

}

int PhotoLibrary::MovieCount() const
{
    return static_cast<int>(std::count_if(files.begin(), files.end(), [](const PhotoFile& file) { return file.IsMovie(); }));
}

int PhotoLibrary::PhotoCount() const
{
    return static_cast<int>(files.size()) - MovieCount();
}

bool PhotoFileKindOfName(std::string_view name, PhotoFileKind& kind)
{
    std::string extension = LowerCaseExtension(name);
    if (extension == "jpg" || extension == "jpeg")
        kind = PhotoFileKind::Photo;
    else if (extension == "mov")
        kind = PhotoFileKind::QuickTimeMovie;
    else if (extension == "avi")
        kind = PhotoFileKind::AviMovie;
    else
        return false;

    return true;
}

bool IsMusicFileName(std::string_view name)
{
    return LowerCaseExtension(name) == "m4a";
}

PhotoLibrary SearchSdCard(Ios::SdCardService& sdCard)
{
    CardSearch search(sdCard);
    return search.Search();
}

}
