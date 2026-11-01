#include "Mii/MiiRecord.h"

#include "Formats/BitFields.h"
#include "Formats/FormatError.h"

#include <format>

namespace Mii
{

namespace
{

// A field of the record: bitCount bits of the 16-bit word at the offset, starting at lowestBit (bit 0 is the lowest).
struct BitField
{
    std::size_t offset;
    int lowestBit;
    int bitCount;
};

constexpr BitField GenderField = {0x00, 14, 1};
constexpr BitField BirthMonthField = {0x00, 10, 4};
constexpr BitField BirthDayField = {0x00, 5, 5};
constexpr BitField FavoriteColorField = {0x00, 1, 4};
constexpr BitField FavoriteField = {0x00, 0, 1};
constexpr std::size_t NameOffset = 0x02;
constexpr std::size_t HeightOffset = 0x16;
constexpr std::size_t WeightOffset = 0x17;
constexpr std::size_t MiiIdOffset = 0x18;
constexpr std::size_t SystemIdOffset = 0x1C;
constexpr BitField FaceShapeField = {0x20, 13, 3};
constexpr BitField SkinColorField = {0x20, 10, 3};
constexpr BitField FacialFeatureField = {0x20, 6, 4};
constexpr BitField MayNotTravelField = {0x20, 2, 1};
constexpr BitField HairTypeField = {0x22, 9, 7};
constexpr BitField HairColorField = {0x22, 6, 3};
constexpr BitField HairMirroredField = {0x22, 5, 1};
constexpr BitField EyebrowTypeField = {0x24, 11, 5};
constexpr BitField EyebrowRotationField = {0x24, 6, 5};
constexpr BitField EyebrowColorField = {0x26, 13, 3};
constexpr BitField EyebrowSizeField = {0x26, 9, 4};
constexpr BitField EyebrowVerticalField = {0x26, 4, 5};
constexpr BitField EyebrowSpacingField = {0x26, 0, 4};
constexpr BitField EyeTypeField = {0x28, 10, 6};
constexpr BitField EyeRotationField = {0x28, 5, 5};
constexpr BitField EyeVerticalField = {0x28, 0, 5};
constexpr BitField EyeColorField = {0x2A, 13, 3};
constexpr BitField EyeSizeField = {0x2A, 9, 4};
constexpr BitField EyeSpacingField = {0x2A, 5, 4};
constexpr BitField NoseTypeField = {0x2C, 12, 4};
constexpr BitField NoseSizeField = {0x2C, 8, 4};
constexpr BitField NoseVerticalField = {0x2C, 3, 5};
constexpr BitField MouthTypeField = {0x2E, 11, 5};
constexpr BitField MouthColorField = {0x2E, 9, 2};
constexpr BitField MouthSizeField = {0x2E, 5, 4};
constexpr BitField MouthVerticalField = {0x2E, 0, 5};
constexpr BitField GlassesTypeField = {0x30, 12, 4};
constexpr BitField GlassesColorField = {0x30, 9, 3};
constexpr BitField GlassesSizeField = {0x30, 5, 4};
constexpr BitField GlassesVerticalField = {0x30, 0, 5};
constexpr BitField MustacheTypeField = {0x32, 14, 2};
constexpr BitField BeardTypeField = {0x32, 12, 2};
constexpr BitField FacialHairColorField = {0x32, 9, 3};
constexpr BitField MustacheSizeField = {0x32, 5, 4};
constexpr BitField MustacheVerticalField = {0x32, 0, 5};
constexpr BitField MoleField = {0x34, 15, 1};
constexpr BitField MoleSizeField = {0x34, 11, 4};
constexpr BitField MoleVerticalField = {0x34, 6, 5};
constexpr BitField MoleHorizontalField = {0x34, 1, 5};
constexpr std::size_t CreatorNameOffset = 0x36;

constexpr int LargestByteValue = 255;

// February always has 29 days, so a Mii born on February 29 is valid in every year.
constexpr std::array<int, MonthCount + 1> DaysInMonth = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

using RecordBytes = std::span<const Formats::Byte, MiiRecordSize>;
using WritableRecordBytes = std::span<Formats::Byte, MiiRecordSize>;

std::uint32_t ReadNumber(RecordBytes bytes, std::size_t offset, std::size_t byteCount)
{
    std::uint32_t number = 0;
    for (std::size_t index = 0; index < byteCount; ++index)
        number = (number << Formats::BitsPerByte) | bytes[offset + index];
    return number;
}

void WriteNumber(WritableRecordBytes bytes, std::size_t offset, std::size_t byteCount, std::uint32_t number)
{
    for (std::size_t index = 0; index < byteCount; ++index)
    {
        std::size_t shift = (byteCount - 1 - index) * Formats::BitsPerByte;
        bytes[offset + index] = static_cast<Formats::Byte>(number >> shift);
    }
}

int ReadField(RecordBytes bytes, BitField field)
{
    std::uint32_t word = ReadNumber(bytes, field.offset, sizeof(std::uint16_t));
    return static_cast<int>(Formats::ExtractBits(word, field.lowestBit, field.bitCount));
}

bool ReadFlag(RecordBytes bytes, BitField field)
{
    return ReadField(bytes, field) != 0;
}

void WriteField(WritableRecordBytes bytes, BitField field, int value)
{
    std::uint32_t largestValue = (1u << field.bitCount) - 1;
    if (value < 0 || static_cast<std::uint32_t>(value) > largestValue)
        throw Formats::FormatError(std::format("Mii record: the value {} does not fit into a field of {} bits", value, field.bitCount));

    std::uint32_t mask = largestValue << field.lowestBit;
    std::uint32_t word = ReadNumber(bytes, field.offset, sizeof(std::uint16_t));
    word = (word & ~mask) | (static_cast<std::uint32_t>(value) << field.lowestBit);
    WriteNumber(bytes, field.offset, sizeof(std::uint16_t), word);
}

void WriteFlag(WritableRecordBytes bytes, BitField field, bool isSet)
{
    WriteField(bytes, field, isSet ? 1 : 0);
}

// A name is up to ten UTF-16 characters. It ends early at a zero character.
std::u16string ReadName(RecordBytes bytes, std::size_t offset)
{
    std::u16string name;
    for (std::size_t index = 0; index < MiiNameLength; ++index)
    {
        auto character = static_cast<char16_t>(ReadNumber(bytes, offset + index * sizeof(char16_t), sizeof(char16_t)));
        if (character == 0)
            break;
        name.push_back(character);
    }
    return name;
}

void WriteName(WritableRecordBytes bytes, std::size_t offset, const std::u16string& name)
{
    if (name.size() > MiiNameLength)
        throw Formats::FormatError(std::format("Mii record: a name has at most {} characters", MiiNameLength));

    for (std::size_t index = 0; index < MiiNameLength; ++index)
    {
        char16_t character = index < name.size() ? name[index] : 0;
        WriteNumber(bytes, offset + index * sizeof(char16_t), sizeof(char16_t), character);
    }
}

bool IsBelow(int value, int count)
{
    return value >= 0 && value < count;
}

bool HasValidFaceAndHair(const MiiRecord& record)
{
    bool hasValidFace = IsBelow(record.face.shape, FaceShapeCount) && IsBelow(record.face.skinColor, SkinColorCount)
        && IsBelow(record.face.facialFeature, FacialFeatureCount);
    bool hasValidHair = IsBelow(record.hair.type, HairTypeCount) && IsBelow(record.hair.color, HairColorCount);
    return hasValidFace && hasValidHair;
}

bool HasValidEyes(const MiiEyes& eyes)
{
    bool hasValidLook = IsBelow(eyes.type, EyeTypeCount) && IsBelow(eyes.color, EyeColorCount) && IsBelow(eyes.size, EyeSizeCount);
    bool hasValidPlace = IsBelow(eyes.rotation, EyeRotationCount) && IsBelow(eyes.spacing, EyeSpacingCount)
        && IsBelow(eyes.vertical, EyeVerticalCount);
    return hasValidLook && hasValidPlace;
}

bool HasValidEyebrows(const MiiEyebrows& eyebrows)
{
    bool hasValidLook = IsBelow(eyebrows.type, EyebrowTypeCount) && IsBelow(eyebrows.color, EyebrowColorCount)
        && IsBelow(eyebrows.size, EyebrowSizeCount);
    bool hasValidPlace = IsBelow(eyebrows.rotation, EyebrowRotationCount) && IsBelow(eyebrows.spacing, EyebrowSpacingCount);
    bool hasValidHeight = eyebrows.vertical >= EyebrowLowestVertical && eyebrows.vertical < EyebrowVerticalCount;
    return hasValidLook && hasValidPlace && hasValidHeight;
}

bool HasValidNoseAndMouth(const MiiRecord& record)
{
    bool hasValidNose = IsBelow(record.nose.type, NoseTypeCount) && IsBelow(record.nose.size, NoseSizeCount)
        && IsBelow(record.nose.vertical, NoseVerticalCount);
    bool hasValidMouth = IsBelow(record.mouth.type, MouthTypeCount) && IsBelow(record.mouth.color, MouthColorCount)
        && IsBelow(record.mouth.size, MouthSizeCount) && IsBelow(record.mouth.vertical, MouthVerticalCount);
    return hasValidNose && hasValidMouth;
}

bool HasValidFacialHair(const MiiFacialHair& facialHair)
{
    return IsBelow(facialHair.mustacheType, MustacheTypeCount) && IsBelow(facialHair.beardType, BeardTypeCount)
        && IsBelow(facialHair.color, FacialHairColorCount) && IsBelow(facialHair.mustacheSize, MustacheSizeCount)
        && IsBelow(facialHair.mustacheVertical, MustacheVerticalCount);
}

bool HasValidGlassesAndMole(const MiiRecord& record)
{
    bool hasValidGlasses = IsBelow(record.glasses.type, GlassesTypeCount) && IsBelow(record.glasses.color, GlassesColorCount)
        && IsBelow(record.glasses.size, GlassesSizeCount) && IsBelow(record.glasses.vertical, GlassesVerticalCount);
    bool hasValidMole = IsBelow(record.mole.size, MoleSizeCount) && IsBelow(record.mole.horizontal, MoleHorizontalCount)
        && IsBelow(record.mole.vertical, MoleVerticalCount);
    return hasValidGlasses && hasValidMole;
}

bool HasValidParts(const MiiRecord& record)
{
    return HasValidFaceAndHair(record) && HasValidEyes(record.eyes) && HasValidEyebrows(record.eyebrows)
        && HasValidNoseAndMouth(record) && HasValidFacialHair(record.facialHair) && HasValidGlassesAndMole(record);
}

bool HasValidBirthday(const MiiRecord& record)
{
    bool hasNoBirthday = record.birthMonth == 0 && record.birthDay == 0;
    if (hasNoBirthday)
        return true;

    if (record.birthMonth < 1 || record.birthMonth > MonthCount)
        return false;

    return record.birthDay >= 1 && record.birthDay <= DaysInMonth[record.birthMonth];
}

}

MiiRecord ReadMiiRecord(std::span<const Formats::Byte, MiiRecordSize> bytes)
{
    MiiRecord record;
    record.gender = ReadFlag(bytes, GenderField) ? Gender::Female : Gender::Male;
    record.birthMonth = ReadField(bytes, BirthMonthField);
    record.birthDay = ReadField(bytes, BirthDayField);
    record.favoriteColor = ReadField(bytes, FavoriteColorField);
    record.isFavorite = ReadFlag(bytes, FavoriteField);
    record.name = ReadName(bytes, NameOffset);
    record.height = bytes[HeightOffset];
    record.weight = bytes[WeightOffset];
    record.miiId = ReadNumber(bytes, MiiIdOffset, sizeof(std::uint32_t));
    record.systemId = ReadNumber(bytes, SystemIdOffset, sizeof(std::uint32_t));

    record.face.shape = ReadField(bytes, FaceShapeField);
    record.face.skinColor = ReadField(bytes, SkinColorField);
    record.face.facialFeature = ReadField(bytes, FacialFeatureField);
    record.mayNotTravel = ReadFlag(bytes, MayNotTravelField);

    record.hair.type = ReadField(bytes, HairTypeField);
    record.hair.color = ReadField(bytes, HairColorField);
    record.hair.isMirrored = ReadFlag(bytes, HairMirroredField);

    record.eyebrows.type = ReadField(bytes, EyebrowTypeField);
    record.eyebrows.rotation = ReadField(bytes, EyebrowRotationField);
    record.eyebrows.color = ReadField(bytes, EyebrowColorField);
    record.eyebrows.size = ReadField(bytes, EyebrowSizeField);
    record.eyebrows.vertical = ReadField(bytes, EyebrowVerticalField);
    record.eyebrows.spacing = ReadField(bytes, EyebrowSpacingField);

    record.eyes.type = ReadField(bytes, EyeTypeField);
    record.eyes.rotation = ReadField(bytes, EyeRotationField);
    record.eyes.vertical = ReadField(bytes, EyeVerticalField);
    record.eyes.color = ReadField(bytes, EyeColorField);
    record.eyes.size = ReadField(bytes, EyeSizeField);
    record.eyes.spacing = ReadField(bytes, EyeSpacingField);

    record.nose.type = ReadField(bytes, NoseTypeField);
    record.nose.size = ReadField(bytes, NoseSizeField);
    record.nose.vertical = ReadField(bytes, NoseVerticalField);

    record.mouth.type = ReadField(bytes, MouthTypeField);
    record.mouth.color = ReadField(bytes, MouthColorField);
    record.mouth.size = ReadField(bytes, MouthSizeField);
    record.mouth.vertical = ReadField(bytes, MouthVerticalField);

    record.glasses.type = ReadField(bytes, GlassesTypeField);
    record.glasses.color = ReadField(bytes, GlassesColorField);
    record.glasses.size = ReadField(bytes, GlassesSizeField);
    record.glasses.vertical = ReadField(bytes, GlassesVerticalField);

    record.facialHair.mustacheType = ReadField(bytes, MustacheTypeField);
    record.facialHair.beardType = ReadField(bytes, BeardTypeField);
    record.facialHair.color = ReadField(bytes, FacialHairColorField);
    record.facialHair.mustacheSize = ReadField(bytes, MustacheSizeField);
    record.facialHair.mustacheVertical = ReadField(bytes, MustacheVerticalField);

    record.mole.isVisible = ReadFlag(bytes, MoleField);
    record.mole.size = ReadField(bytes, MoleSizeField);
    record.mole.vertical = ReadField(bytes, MoleVerticalField);
    record.mole.horizontal = ReadField(bytes, MoleHorizontalField);

    record.creatorName = ReadName(bytes, CreatorNameOffset);
    return record;
}

void WriteMiiRecord(const MiiRecord& record, std::span<Formats::Byte, MiiRecordSize> destination)
{
    if (!IsBelow(record.height, LargestByteValue + 1) || !IsBelow(record.weight, LargestByteValue + 1))
        throw Formats::FormatError("Mii record: height and weight are single bytes");

    WriteFlag(destination, GenderField, record.gender == Gender::Female);
    WriteField(destination, BirthMonthField, record.birthMonth);
    WriteField(destination, BirthDayField, record.birthDay);
    WriteField(destination, FavoriteColorField, record.favoriteColor);
    WriteFlag(destination, FavoriteField, record.isFavorite);
    WriteName(destination, NameOffset, record.name);
    destination[HeightOffset] = static_cast<Formats::Byte>(record.height);
    destination[WeightOffset] = static_cast<Formats::Byte>(record.weight);
    WriteNumber(destination, MiiIdOffset, sizeof(std::uint32_t), record.miiId);
    WriteNumber(destination, SystemIdOffset, sizeof(std::uint32_t), record.systemId);

    WriteField(destination, FaceShapeField, record.face.shape);
    WriteField(destination, SkinColorField, record.face.skinColor);
    WriteField(destination, FacialFeatureField, record.face.facialFeature);
    WriteFlag(destination, MayNotTravelField, record.mayNotTravel);

    WriteField(destination, HairTypeField, record.hair.type);
    WriteField(destination, HairColorField, record.hair.color);
    WriteFlag(destination, HairMirroredField, record.hair.isMirrored);

    WriteField(destination, EyebrowTypeField, record.eyebrows.type);
    WriteField(destination, EyebrowRotationField, record.eyebrows.rotation);
    WriteField(destination, EyebrowColorField, record.eyebrows.color);
    WriteField(destination, EyebrowSizeField, record.eyebrows.size);
    WriteField(destination, EyebrowVerticalField, record.eyebrows.vertical);
    WriteField(destination, EyebrowSpacingField, record.eyebrows.spacing);

    WriteField(destination, EyeTypeField, record.eyes.type);
    WriteField(destination, EyeRotationField, record.eyes.rotation);
    WriteField(destination, EyeVerticalField, record.eyes.vertical);
    WriteField(destination, EyeColorField, record.eyes.color);
    WriteField(destination, EyeSizeField, record.eyes.size);
    WriteField(destination, EyeSpacingField, record.eyes.spacing);

    WriteField(destination, NoseTypeField, record.nose.type);
    WriteField(destination, NoseSizeField, record.nose.size);
    WriteField(destination, NoseVerticalField, record.nose.vertical);

    WriteField(destination, MouthTypeField, record.mouth.type);
    WriteField(destination, MouthColorField, record.mouth.color);
    WriteField(destination, MouthSizeField, record.mouth.size);
    WriteField(destination, MouthVerticalField, record.mouth.vertical);

    WriteField(destination, GlassesTypeField, record.glasses.type);
    WriteField(destination, GlassesColorField, record.glasses.color);
    WriteField(destination, GlassesSizeField, record.glasses.size);
    WriteField(destination, GlassesVerticalField, record.glasses.vertical);

    WriteField(destination, MustacheTypeField, record.facialHair.mustacheType);
    WriteField(destination, BeardTypeField, record.facialHair.beardType);
    WriteField(destination, FacialHairColorField, record.facialHair.color);
    WriteField(destination, MustacheSizeField, record.facialHair.mustacheSize);
    WriteField(destination, MustacheVerticalField, record.facialHair.mustacheVertical);

    WriteFlag(destination, MoleField, record.mole.isVisible);
    WriteField(destination, MoleSizeField, record.mole.size);
    WriteField(destination, MoleVerticalField, record.mole.vertical);
    WriteField(destination, MoleHorizontalField, record.mole.horizontal);

    WriteName(destination, CreatorNameOffset, record.creatorName);
}

bool IsValidMii(const MiiRecord& record)
{
    if (!HasValidParts(record))
        return false;

    if (record.name.empty())
        return false;

    if (record.height > MaximumHeight || record.weight > MaximumWeight)
        return false;

    if (!HasValidBirthday(record))
        return false;

    if (!IsBelow(record.favoriteColor, FavoriteColorCount))
        return false;

    if (IsSpecialMii(record) && !record.mayNotTravel)
        return false;

    return true;
}

bool HasEmptyCreateId(const MiiRecord& record)
{
    return record.miiId == 0 && record.systemId == 0;
}

bool IsTemporaryMii(const MiiRecord& record)
{
    return (record.miiId & TemporaryMiiFlag) != 0;
}

bool IsSpecialMii(const MiiRecord& record)
{
    if (HasEmptyCreateId(record) || IsTemporaryMii(record))
        return false;

    return (record.miiId & NormalMiiFlag) == 0;
}

bool IsUsableMii(const MiiRecord& record)
{
    return !HasEmptyCreateId(record) && IsValidMii(record) && !IsTemporaryMii(record);
}

bool WasMadeOnConsole(const MiiRecord& record, std::uint32_t consoleSystemId)
{
    if (HasEmptyCreateId(record) || IsTemporaryMii(record))
        return false;

    if ((record.miiId & ForeignMiiFlag) != 0)
        return false;

    return record.systemId == consoleSystemId;
}

}
