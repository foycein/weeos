#pragma once

#include "Ios/Title/TitleId.h"

#include <array>
#include <cstdint>

namespace SystemMenu
{

// The kind of a title is the high half of its title id. The grid shows titles of these six kinds only.
constexpr std::uint32_t GameSaveKind = 0x00010000;
constexpr std::uint32_t DownloadedChannelKind = 0x00010001;
constexpr std::uint32_t SystemChannelKind = 0x00010002;

constexpr std::array<std::uint32_t, 6> KindsShownInTheGrid = {
    GameSaveKind, DownloadedChannelKind, SystemChannelKind, 0x00010003, 0x00010004, 0x00010006};

bool IsKindShownInTheGrid(std::uint32_t kind);

// The low half of a channel's title id has four letters, for example 'H' 'A' 'C' 'A' for the Mii Channel. The first three letters
// name the channel, the last one the version of the channel: the letter A marks the first one, which the console ships with,
// and a letter such as E (an update, in English) replaces it.
constexpr std::uint32_t FamilyMask = 0xFFFFFF00;
constexpr std::uint32_t FirstVersionLetter = 'A';

// Two titles are versions of one channel when their kinds and the first three letters of their names are the same.
bool AreInOneFamily(Ios::TitleId first, Ios::TitleId second);

// True for the first version of a channel, the letter A.
bool IsFirstVersion(Ios::TitleId titleId);

// True when the title has this kind and its name begins with the three letters of the family.
bool IsInFamily(Ios::TitleId titleId, std::uint32_t kind, std::uint32_t family);

// The first three letters of the Photo Channel and its two later versions: HAA, HAY (Photo Channel 1.1) and HAZ.
constexpr std::uint32_t FirstPhotoChannelFamily = 0x48414100;
constexpr std::uint32_t PhotoChannel11Family = 0x48415900;
constexpr std::uint32_t PhotoChannelSaveFamily = 0x48415A00;

// The first three letters of the Wii Shop Channel (HAB), the Forecast Channel (HAF) and the News Channel (HAG).
constexpr std::uint32_t ShopChannelFamily = 0x48414200;
constexpr std::uint32_t ForecastChannelFamily = 0x48414600;
constexpr std::uint32_t NewsChannelFamily = 0x48414700;

}
