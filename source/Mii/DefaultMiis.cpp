#include "Mii/DefaultMiis.h"

#include "Formats/FormatError.h"

#include <array>
#include <format>

namespace Mii
{

namespace
{

// What tells the six default Miis apart. The eyebrow color is the hair color.
struct DefaultMiiLook
{
    Gender gender;
    int favoriteColor;
    int skinColor;
    int hairType;
    int hairColor;
    int eyebrowType;
    int eyeType;
    int eyeRotation;
    int eyeColor;
};

constexpr std::array<DefaultMiiLook, DefaultMiiCount> DefaultMiiLooks = {{
    {Gender::Male, 4, 4, 68, 0, 6, 2, 4, 0},
    {Gender::Male, 5, 0, 55, 6, 6, 2, 4, 4},
    {Gender::Male, 0, 1, 33, 1, 6, 2, 4, 0},
    {Gender::Female, 2, 2, 24, 0, 0, 4, 3, 0},
    {Gender::Female, 6, 0, 14, 7, 0, 4, 3, 5},
    {Gender::Female, 7, 0, 12, 1, 0, 4, 3, 0},
}};

constexpr std::uint32_t FirstDefaultMiiId = 0x80000000;
constexpr std::uint32_t DefaultSystemId = 0xECFF82D2;

// The parts all six share.
MiiRecord MakeCommonDefaultMii()
{
    MiiRecord record;
    record.name = u"no name";
    record.height = 64;
    record.weight = 64;
    record.systemId = DefaultSystemId;
    record.mayNotTravel = true;

    record.eyebrows.rotation = 6;
    record.eyebrows.size = 4;
    record.eyebrows.vertical = 10;
    record.eyebrows.spacing = 2;

    record.eyes.vertical = 12;
    record.eyes.size = 4;
    record.eyes.spacing = 2;

    record.nose.type = 1;
    record.nose.size = 4;
    record.nose.vertical = 9;

    record.mouth.type = 23;
    record.mouth.size = 4;
    record.mouth.vertical = 13;

    record.glasses.size = 4;
    record.glasses.vertical = 10;

    record.facialHair.mustacheSize = 4;
    record.facialHair.mustacheVertical = 10;

    record.mole.size = 4;
    record.mole.vertical = 20;
    record.mole.horizontal = 2;
    return record;
}

}

MiiRecord DefaultMii(std::size_t number)
{
    if (number >= DefaultMiiCount)
        throw Formats::FormatError(std::format("There is no default Mii {}, there are {}", number, DefaultMiiCount));

    const DefaultMiiLook& look = DefaultMiiLooks[number];

    MiiRecord record = MakeCommonDefaultMii();
    record.miiId = FirstDefaultMiiId + static_cast<std::uint32_t>(number);
    record.gender = look.gender;
    record.favoriteColor = look.favoriteColor;
    record.face.skinColor = look.skinColor;
    record.hair.type = look.hairType;
    record.hair.color = look.hairColor;
    record.eyebrows.type = look.eyebrowType;
    record.eyebrows.color = look.hairColor;
    record.eyes.type = look.eyeType;
    record.eyes.rotation = look.eyeRotation;
    record.eyes.color = look.eyeColor;
    return record;
}

}
