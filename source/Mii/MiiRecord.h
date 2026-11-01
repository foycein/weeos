#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace Mii
{

// A Mii as the Wii stores it: 74 bytes of bit fields (docs/research/Mii.md section 1).
constexpr std::size_t MiiRecordSize = 74;
constexpr std::size_t MiiNameLength = 10;

// How many values each part has. A value is valid when it is below the count.
constexpr int FavoriteColorCount = 12;
constexpr int MonthCount = 12;
constexpr int MaximumHeight = 128;
constexpr int MaximumWeight = 128;
constexpr int FaceShapeCount = 8;
constexpr int SkinColorCount = 6;
constexpr int FacialFeatureCount = 12;
constexpr int HairTypeCount = 72;
constexpr int HairColorCount = 8;
constexpr int EyebrowTypeCount = 24;
constexpr int EyebrowRotationCount = 12;
constexpr int EyebrowColorCount = 8;
constexpr int EyebrowSizeCount = 9;
constexpr int EyebrowLowestVertical = 3;
constexpr int EyebrowVerticalCount = 19;
constexpr int EyebrowSpacingCount = 13;
constexpr int EyeTypeCount = 48;
constexpr int EyeRotationCount = 8;
constexpr int EyeVerticalCount = 19;
constexpr int EyeColorCount = 6;
constexpr int EyeSizeCount = 8;
constexpr int EyeSpacingCount = 13;
constexpr int NoseTypeCount = 12;
constexpr int NoseSizeCount = 9;
constexpr int NoseVerticalCount = 19;
constexpr int MouthTypeCount = 24;
constexpr int MouthColorCount = 3;
constexpr int MouthSizeCount = 9;
constexpr int MouthVerticalCount = 19;
constexpr int GlassesTypeCount = 9;
constexpr int GlassesColorCount = 6;
constexpr int GlassesSizeCount = 8;
constexpr int GlassesVerticalCount = 21;
constexpr int MustacheTypeCount = 4;
constexpr int BeardTypeCount = 4;
constexpr int FacialHairColorCount = 8;
constexpr int MustacheSizeCount = 9;
constexpr int MustacheVerticalCount = 17;
constexpr int MoleSizeCount = 9;
constexpr int MoleVerticalCount = 31;
constexpr int MoleHorizontalCount = 17;

// The top bits of the Mii id. A normal Mii has the first one; a special ("gold pants") Mii does not.
constexpr std::uint32_t NormalMiiFlag = 0x80000000;
constexpr std::uint32_t ForeignMiiFlag = 0x40000000;
constexpr std::uint32_t TemporaryMiiFlag = 0x20000000;

enum class Gender
{
    Male,
    Female,
};

struct MiiFace
{
    int shape = 0;
    int skinColor = 0;

    // Make-up and wrinkles.
    int facialFeature = 0;
};

struct MiiHair
{
    int type = 0;
    int color = 0;

    // The hair is parted on the other side: hair, forehead and cap are mirrored.
    bool isMirrored = false;
};

struct MiiEyebrows
{
    int type = 0;
    int rotation = 0;
    int color = 0;
    int size = 0;
    int vertical = 0;
    int spacing = 0;
};

struct MiiEyes
{
    int type = 0;
    int rotation = 0;
    int vertical = 0;
    int color = 0;
    int size = 0;
    int spacing = 0;
};

struct MiiNose
{
    int type = 0;
    int size = 0;
    int vertical = 0;
};

struct MiiMouth
{
    int type = 0;
    int color = 0;
    int size = 0;
    int vertical = 0;
};

struct MiiGlasses
{
    int type = 0;
    int color = 0;
    int size = 0;
    int vertical = 0;
};

struct MiiFacialHair
{
    int mustacheType = 0;
    int beardType = 0;
    int color = 0;
    int mustacheSize = 0;
    int mustacheVertical = 0;
};

struct MiiMole
{
    bool isVisible = false;
    int size = 0;
    int vertical = 0;
    int horizontal = 0;
};

// The fields of a Mii record. A month and a day of 0 mean that the Mii has no birthday.
struct MiiRecord
{
    Gender gender = Gender::Male;
    int birthMonth = 0;
    int birthDay = 0;
    int favoriteColor = 0;
    bool isFavorite = false;
    std::u16string name;
    int height = 0;
    int weight = 0;

    // Together the two ids are the create id that tells Miis apart. Both zero means an empty slot.
    std::uint32_t miiId = 0;
    std::uint32_t systemId = 0;

    // The Mii does not go to the Mii Parade of other consoles ("mingle" off).
    bool mayNotTravel = false;

    MiiFace face;
    MiiHair hair;
    MiiEyebrows eyebrows;
    MiiEyes eyes;
    MiiNose nose;
    MiiMouth mouth;
    MiiGlasses glasses;
    MiiFacialHair facialHair;
    MiiMole mole;
    std::u16string creatorName;
};

MiiRecord ReadMiiRecord(std::span<const Formats::Byte, MiiRecordSize> bytes);

// Writes only the bits the face library uses. The other bits of the destination stay as they were,
// because real records carry leftovers there and the original keeps them too.
void WriteMiiRecord(const MiiRecord& record, std::span<Formats::Byte, MiiRecordSize> destination);

// The check of the face library: every part in its range, a name, height and weight up to 128, a real birthday or none,
// and a special Mii must not travel.
bool IsValidMii(const MiiRecord& record);

bool HasEmptyCreateId(const MiiRecord& record);
bool IsTemporaryMii(const MiiRecord& record);
bool IsSpecialMii(const MiiRecord& record);

// A Mii that a program may show: it has a create id, it is valid and it is not a temporary one.
bool IsUsableMii(const MiiRecord& record);

}
