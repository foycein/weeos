#include "SystemMenu/SdCardMenu/SdChannelPlaces.h"

#include "SystemMenu/DataManagement/BackupArchive.h"
#include "SystemMenu/DataManagement/DataFiles.h"
#include "SystemMenu/DataManagement/DataLists.h"

#include "Formats/FormatError.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace SystemMenu
{

namespace
{

constexpr std::string_view MainPlacesFilePath = "/private/wii/loc.dat";
constexpr std::string_view CopyPlacesFilePath = "/private/wii/loc.bak";
constexpr std::array<std::string_view, 2> PlacesFilePaths = {MainPlacesFilePath, CopyPlacesFilePath};

// The Photo Channel 1.1, which the SD Card Menu leaves out.
constexpr std::uint32_t PhotoChannelOneOne = 0x48415A41;

constexpr std::uint32_t EmptyPlace = 0;

std::optional<Formats::ChannelPlaces> ReadPlacesFile(DataManagementServices& services, std::string_view path)
{
    try
    {
        Formats::Bytes encrypted = ReadSdFile(services.sdCard, MakeSdPath(path));
        if (encrypted.size() != Formats::ChannelPlacesFileSize)
            return std::nullopt;

        Formats::Bytes plainFile = DecryptFromCard(services.identity, encrypted);
        return Formats::ReadChannelPlaces(plainFile, services.identity.GetSdCardDigestPlaceholder());
    }
    catch (const DataError&)
    {
        return std::nullopt;
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
}

bool IsOnCard(const std::vector<std::uint32_t>& codes, std::uint32_t code)
{
    return std::find(codes.begin(), codes.end(), code) != codes.end();
}

std::vector<std::uint32_t> ChannelsShownOfCard(DataManagementServices& services)
{
    std::vector<std::uint32_t> codes = ListChannelsOnCard(services);
    std::erase(codes, PhotoChannelOneOne);
    return codes;
}

}

SdCardChannels PlaceChannelsOfCard(DataManagementServices& services)
{
    SdCardChannels channels;

    std::optional<Formats::ChannelPlaces> mainFile = ReadPlacesFile(services, MainPlacesFilePath);
    std::optional<Formats::ChannelPlaces> copyFile = ReadPlacesFile(services, CopyPlacesFilePath);
    channels.mustBeWritten = !mainFile || !copyFile;
    if (mainFile)
        channels.places = *mainFile;
    else if (copyFile)
        channels.places = *copyFile;

    std::vector<std::uint32_t> codes = ChannelsShownOfCard(services);
    for (std::uint32_t& code : channels.places)
    {
        if (code != EmptyPlace && !IsOnCard(codes, code))
        {
            code = EmptyPlace;
            channels.mustBeWritten = true;
        }
    }

    for (std::uint32_t code : codes)
    {
        bool hasPlace = std::find(channels.places.begin(), channels.places.end(), code) != channels.places.end();
        if (hasPlace)
            continue;

        auto emptyPlace = std::find(channels.places.begin(), channels.places.end(), EmptyPlace);
        if (emptyPlace == channels.places.end())
        {
            channels.hasTooManyChannels = true;
            break;
        }

        *emptyPlace = code;
        channels.mustBeWritten = true;
    }
    return channels;
}

DataResult WriteChannelPlaces(DataManagementServices& services, const Formats::ChannelPlaces& places)
{
    try
    {
        Formats::Bytes plainFile = Formats::WriteChannelPlaces(places, services.identity.GetSdCardDigestPlaceholder());
        Formats::Bytes encrypted = EncryptForCard(services.identity, plainFile);
        for (std::string_view path : PlacesFilePaths)
        {
            Ios::SdPath file = MakeSdPath(path);
            MakeSdFolders(services.sdCard, file.Parent());
            WriteSdFile(services.sdCard, file, encrypted);
        }
        return DataResult::Done;
    }
    catch (const DataError& error)
    {
        return error.Result();
    }
}

bool MoveChannelToPlace(Formats::ChannelPlaces& places, std::size_t fromPlace, std::size_t toPlace)
{
    if (places.at(fromPlace) == EmptyPlace || places.at(toPlace) != EmptyPlace)
        return false;

    places[toPlace] = places[fromPlace];
    places[fromPlace] = EmptyPlace;
    return true;
}

}
