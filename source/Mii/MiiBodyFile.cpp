#include "Mii/MiiBodyFile.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/TextureDecoder.h"
#include "Formats/TextureFormat.h"
#include "Formats/U8Archive.h"
#include "Formats/Yaz0.h"

#include <format>

namespace Mii
{

namespace
{

// The NW4R resource file (BRRES): a root folder of index groups, each entry with a name and the offset of its data.
constexpr std::size_t RootOffsetPosition = 0x0C;
constexpr std::size_t RootGroupInsideRoot = 8;
constexpr std::size_t GroupCountPosition = 4;
constexpr std::size_t FirstGroupEntry = 8;
constexpr std::size_t GroupEntrySize = 16;
constexpr std::size_t EntryNamePosition = 8;
constexpr std::size_t EntryDataPosition = 12;

constexpr std::string_view ModelGroupName = "3DModels(NW4R)";
constexpr std::string_view TextureGroupName = "Textures(NW4R)";
constexpr std::string_view AnimationGroupName = "AnmChr(NW4R)";
constexpr std::string_view StandingAnimationName = "bodyWait";
constexpr std::string_view EnvironmentTextureName = "env0";
constexpr std::string_view ShirtArraySuffix = "__outer";
constexpr std::string_view PantsArraySuffix = "__pants";

// The model (MDL0, version 8) lists the groups of its parts at 0x10.
constexpr std::size_t ModelSectionsPosition = 0x10;
constexpr std::size_t BoneSection = 1;
constexpr std::size_t PositionSection = 2;
constexpr std::size_t NormalSection = 3;
constexpr std::size_t ObjectSection = 8;

constexpr std::size_t BoneIndexPosition = 0x0C;
constexpr std::size_t BoneMatrixPosition = 0x10;
constexpr std::size_t BoneFlagsPosition = 0x14;
constexpr std::size_t BoneScalePosition = 0x20;
constexpr std::size_t BoneRotationPosition = 0x2C;
constexpr std::size_t BoneTranslationPosition = 0x38;
constexpr std::size_t BoneParentPosition = 0x5C;
constexpr std::uint32_t BoneUndoesParentScaleFlag = 0x20;

constexpr std::size_t ArrayDataPosition = 0x08;
constexpr std::size_t ArrayIndexPosition = 0x10;
constexpr std::size_t ArrayFormatPosition = 0x18;
constexpr std::size_t ArrayCountPosition = 0x1E;
constexpr std::uint32_t FloatFormat = 4;

constexpr std::size_t ObjectSingleBonePosition = 0x08;
constexpr std::size_t ObjectDescriptorPosition = 0x0C;
constexpr std::size_t ObjectListSizePosition = 0x28;
constexpr std::size_t ObjectListPosition = 0x24;
constexpr std::size_t ObjectListOffsetPosition = 0x2C;
constexpr std::size_t ObjectPositionArrayPosition = 0x48;
constexpr std::size_t ObjectNormalArrayPosition = 0x4A;

// Every corner of the body is four bytes: a matrix slot, a texture matrix slot, a position number and a normal number.
constexpr std::uint32_t BodyCornerDescriptor = 0x1403;
constexpr std::size_t BodyCornerSize = 4;
constexpr std::size_t CornerPositionByte = 2;
constexpr std::size_t CornerNormalByte = 3;

// The commands of a draw list.
constexpr std::uint8_t NoOperation = 0x00;
constexpr std::uint8_t LoadPositionMatrix = 0x20;
constexpr std::uint8_t LoadNormalMatrix = 0x28;
constexpr std::uint8_t LoadTextureMatrix = 0x30;
constexpr std::uint8_t TrianglesCommand = 0x90;
constexpr std::uint8_t TriangleStripCommand = 0x98;
constexpr std::uint8_t CommandMask = 0xF8;
constexpr std::uint32_t MatrixAddressMask = 0xFFF;
constexpr std::uint32_t MatrixAddressStep = 12;
constexpr int MatrixIdShift = 16;
constexpr std::uint32_t MatrixSlotStep = 3;

// The animation (CHR0, version 3): a group of nodes, one per bone.
constexpr std::size_t AnimationNodesPosition = 0x10;
constexpr std::size_t NodeCodePosition = 4;
constexpr std::size_t NodeValuesPosition = 8;
constexpr int HasScaleBit = 22;
constexpr int HasRotationBit = 23;
constexpr int HasTranslationBit = 24;
constexpr int FirstFixedScaleBit = 13;
constexpr int FirstFixedRotationBit = 16;
constexpr int FirstFixedTranslationBit = 19;
constexpr std::uint32_t UniformScaleFlag = 0x10;
constexpr std::uint32_t UndoesParentScaleFlag = 0x400;
constexpr int RotationFormatShift = 25;
constexpr std::uint32_t RotationFormatMask = 3;
constexpr std::size_t AxisCount = 3;

// A track: a count, then keys from 8 on. The keys come in three sizes.
constexpr std::size_t FirstKeyPosition = 8;
constexpr std::uint32_t SmallRotationKeys = 1;
constexpr std::uint32_t MiddleRotationKeys = 2;
constexpr std::uint32_t FloatKeys = 3;
constexpr float DegreesPerTurn = 360.0f;
constexpr float SmallKeyStepsPerTurn = 4096.0f;
constexpr float MiddleKeyStepsPerTurn = 65536.0f;
constexpr int SmallKeyFrameShift = 24;
constexpr int SmallKeyAngleShift = 12;
constexpr std::uint32_t SmallKeyAngleMask = 0xFFF;
constexpr std::uint32_t SmallKeyAngleSignBit = 0x800;
constexpr int SmallKeyAngleRange = 0x1000;

constexpr std::size_t TextureDataPosition = 0x10;
constexpr std::size_t TextureWidthPosition = 0x1C;
constexpr std::size_t TextureHeightPosition = 0x1E;
constexpr std::size_t TextureFormatPosition = 0x20;

constexpr std::string_view MaleBodyFileName = "maleBody.brres";
constexpr std::string_view FemaleBodyFileName = "femaleBody.brres";

// Reads numbers at given places of the file. Every read is checked.
class FileView
{
public:
    explicit FileView(Formats::ByteView bytes)
        : reader(bytes, "Mii body file")
    {
    }

    std::uint8_t Byte(std::size_t offset)
    {
        reader.MoveTo(offset);
        return reader.ReadByte();
    }

    std::uint16_t Uint16(std::size_t offset)
    {
        reader.MoveTo(offset);
        return reader.ReadUint16();
    }

    std::int16_t Int16(std::size_t offset)
    {
        reader.MoveTo(offset);
        return reader.ReadInt16();
    }

    std::uint32_t Uint32(std::size_t offset)
    {
        reader.MoveTo(offset);
        return reader.ReadUint32();
    }

    std::int32_t Int32(std::size_t offset)
    {
        return static_cast<std::int32_t>(Uint32(offset));
    }

    float Float(std::size_t offset)
    {
        reader.MoveTo(offset);
        return reader.ReadFloat();
    }

    Formats::Vector3 Vector(std::size_t offset)
    {
        return {Float(offset), Float(offset + sizeof(float)), Float(offset + 2 * sizeof(float))};
    }

    std::string Name(std::size_t offset)
    {
        reader.MoveTo(offset);
        return reader.ReadTextUntilZero();
    }

    Formats::ByteView Bytes(std::size_t offset, std::size_t size)
    {
        reader.MoveTo(offset);
        return reader.ReadBytes(size);
    }

    [[noreturn]] void Fail(std::string_view problem) const
    {
        reader.Fail(problem);
    }

private:
    Formats::ByteReader reader;
};

struct GroupEntry
{
    std::string name;
    std::size_t offset;
};

// Offsets in a group count from the start of the group; the first entry is the group's own root and is skipped.
std::vector<GroupEntry> ReadGroup(FileView& file, std::size_t groupOffset)
{
    std::uint32_t count = file.Uint32(groupOffset + GroupCountPosition);

    std::vector<GroupEntry> entries;
    for (std::uint32_t entryNumber = 1; entryNumber <= count; ++entryNumber)
    {
        std::size_t entryOffset = groupOffset + FirstGroupEntry + entryNumber * GroupEntrySize;
        GroupEntry entry;
        entry.name = file.Name(groupOffset + file.Int32(entryOffset + EntryNamePosition));
        entry.offset = groupOffset + file.Int32(entryOffset + EntryDataPosition);
        entries.push_back(entry);
    }
    return entries;
}

std::size_t FindInGroup(FileView& file, std::size_t groupOffset, std::string_view name)
{
    for (const GroupEntry& entry : ReadGroup(file, groupOffset))
    {
        if (entry.name == name)
            return entry.offset;
    }
    file.Fail(std::format("there is no {}", name));
}

std::size_t ModelSectionGroup(FileView& file, std::size_t modelOffset, std::size_t section)
{
    return modelOffset + file.Int32(modelOffset + ModelSectionsPosition + section * sizeof(std::uint32_t));
}

std::vector<BodyBone> ReadBones(FileView& file, std::size_t modelOffset)
{
    std::vector<BodyBone> bones;
    for (const GroupEntry& entry : ReadGroup(file, ModelSectionGroup(file, modelOffset, BoneSection)))
    {
        std::size_t bone = entry.offset;
        if (file.Uint32(bone + BoneIndexPosition) != bones.size())
            file.Fail(std::format("the bone {} is not in the order of its number", entry.name));

        BodyBone bodyBone;
        bodyBone.name = entry.name;
        bodyBone.matrixId = file.Int32(bone + BoneMatrixPosition);
        bodyBone.restTransform.scale = file.Vector(bone + BoneScalePosition);
        bodyBone.restTransform.rotationDegrees = file.Vector(bone + BoneRotationPosition);
        bodyBone.restTransform.translation = file.Vector(bone + BoneTranslationPosition);
        bodyBone.restTransform.undoesParentScale = (file.Uint32(bone + BoneFlagsPosition) & BoneUndoesParentScaleFlag) != 0;

        std::int32_t parentOffset = file.Int32(bone + BoneParentPosition);
        if (parentOffset != 0)
            bodyBone.parent = static_cast<int>(file.Uint32(bone + parentOffset + BoneIndexPosition));
        if (bodyBone.parent >= static_cast<int>(bones.size()))
            file.Fail(std::format("the bone {} comes before its parent", entry.name));

        bones.push_back(bodyBone);
    }
    return bones;
}

struct VectorArray
{
    std::string name;
    std::vector<Formats::Vector3> vectors;
};

// The position or normal arrays of the model, by their numbers.
std::map<int, VectorArray> ReadVectorArrays(FileView& file, std::size_t groupOffset)
{
    std::map<int, VectorArray> arrays;
    for (const GroupEntry& entry : ReadGroup(file, groupOffset))
    {
        std::size_t header = entry.offset;
        if (file.Uint32(header + ArrayFormatPosition) != FloatFormat)
            file.Fail(std::format("the array {} does not hold floats", entry.name));

        VectorArray array;
        array.name = entry.name;
        std::size_t data = header + file.Int32(header + ArrayDataPosition);
        std::uint16_t count = file.Uint16(header + ArrayCountPosition);
        for (std::uint16_t vectorNumber = 0; vectorNumber < count; ++vectorNumber)
            array.vectors.push_back(file.Vector(data + vectorNumber * sizeof(Formats::Vector3)));

        arrays[static_cast<int>(file.Uint32(header + ArrayIndexPosition))] = array;
    }
    return arrays;
}

// A strip makes a triangle of every three corners in a row; every second one has its first two corners swapped.
void AddStrip(const std::vector<BodyCorner>& strip, std::vector<BodyCorner>& triangleCorners)
{
    for (std::size_t first = 0; first + 2 < strip.size(); ++first)
    {
        bool isSecondOfPair = first % 2 == 1;
        if (isSecondOfPair)
        {
            triangleCorners.push_back(strip[first + 1]);
            triangleCorners.push_back(strip[first]);
        }
        else
        {
            triangleCorners.push_back(strip[first]);
            triangleCorners.push_back(strip[first + 1]);
        }
        triangleCorners.push_back(strip[first + 2]);
    }
}

// Follows the draw list in order: matrix loads fill slots, and every corner names its bone through a slot.
std::vector<BodyCorner> ReadDrawList(FileView& file, std::size_t listStart, std::size_t listSize)
{
    std::map<std::uint32_t, int> matrixIdOfSlot;
    std::vector<BodyCorner> triangleCorners;

    std::size_t position = listStart;
    std::size_t end = listStart + listSize;
    while (position < end)
    {
        std::uint8_t command = file.Byte(position);
        ++position;

        if (command == NoOperation)
            continue;

        if (command == LoadPositionMatrix || command == LoadNormalMatrix || command == LoadTextureMatrix)
        {
            std::uint32_t word = file.Uint32(position);
            position += sizeof(std::uint32_t);
            if (command == LoadPositionMatrix)
                matrixIdOfSlot[(word & MatrixAddressMask) / MatrixAddressStep] = static_cast<int>(word >> MatrixIdShift);
            continue;
        }

        std::uint8_t primitive = command & CommandMask;
        if (primitive != TrianglesCommand && primitive != TriangleStripCommand)
            file.Fail(std::format("the draw list has the unknown command {:#x}", command));

        std::uint16_t cornerCount = file.Uint16(position);
        position += sizeof(std::uint16_t);

        std::vector<BodyCorner> corners;
        for (std::uint16_t cornerNumber = 0; cornerNumber < cornerCount; ++cornerNumber)
        {
            std::uint32_t slot = file.Byte(position) / MatrixSlotStep;
            if (!matrixIdOfSlot.contains(slot))
                file.Fail("a corner uses a matrix slot that was never loaded");

            BodyCorner corner;
            corner.matrixId = matrixIdOfSlot[slot];
            corner.position = file.Byte(position + CornerPositionByte);
            corner.normal = file.Byte(position + CornerNormalByte);
            corners.push_back(corner);
            position += BodyCornerSize;
        }

        if (primitive == TriangleStripCommand)
            AddStrip(corners, triangleCorners);
        else
            triangleCorners.insert(triangleCorners.end(), corners.begin(), corners.end());
    }
    return triangleCorners;
}

// Reads the shirt and the pants. The shadow is bound to one bone as a whole and is left out.
void ReadParts(FileView& file, std::size_t modelOffset, MiiBodyFile& body)
{
    std::map<int, VectorArray> positionArrays = ReadVectorArrays(file, ModelSectionGroup(file, modelOffset, PositionSection));
    std::map<int, VectorArray> normalArrays = ReadVectorArrays(file, ModelSectionGroup(file, modelOffset, NormalSection));

    for (const GroupEntry& entry : ReadGroup(file, ModelSectionGroup(file, modelOffset, ObjectSection)))
    {
        std::size_t object = entry.offset;
        bool isBoundToOneBone = file.Int32(object + ObjectSingleBonePosition) >= 0;
        if (isBoundToOneBone)
            continue;

        if (file.Uint32(object + ObjectDescriptorPosition) != BodyCornerDescriptor)
            file.Fail(std::format("the object {} does not have the corners of a body", entry.name));

        const VectorArray& positions = positionArrays.at(file.Int16(object + ObjectPositionArrayPosition));
        const VectorArray& normals = normalArrays.at(file.Int16(object + ObjectNormalArrayPosition));

        std::size_t listStart = object + ObjectListPosition + file.Int32(object + ObjectListOffsetPosition);
        BodyPart part;
        part.positions = positions.vectors;
        part.normals = normals.vectors;
        part.triangleCorners = ReadDrawList(file, listStart, file.Uint32(object + ObjectListSizePosition));

        if (positions.name.ends_with(ShirtArraySuffix))
            body.shirt = part;
        else if (positions.name.ends_with(PantsArraySuffix))
            body.pants = part;
    }

    if (body.shirt.triangleCorners.empty() || body.pants.triangleCorners.empty())
        file.Fail("the model has no shirt or no pants");
}

// The value of a track at frame 0, which is its first key.
float FirstKeyValue(FileView& file, std::size_t track, std::uint32_t keyFormat)
{
    std::size_t key = track + FirstKeyPosition;

    if (keyFormat == SmallRotationKeys)
    {
        std::uint32_t word = file.Uint32(key);
        int angle = static_cast<int>((word >> SmallKeyAngleShift) & SmallKeyAngleMask);
        if ((angle & SmallKeyAngleSignBit) != 0)
            angle -= SmallKeyAngleRange;
        if ((word >> SmallKeyFrameShift) != 0)
            file.Fail("a track has no key at frame 0");
        return angle * DegreesPerTurn / SmallKeyStepsPerTurn;
    }

    if (keyFormat == MiddleRotationKeys)
    {
        if (file.Uint16(key) != 0)
            file.Fail("a track has no key at frame 0");
        return file.Int16(key + sizeof(std::uint16_t)) * DegreesPerTurn / MiddleKeyStepsPerTurn;
    }

    if (file.Float(key) != 0.0f)
        file.Fail("a track has no key at frame 0");
    return file.Float(key + sizeof(float));
}

// The values of one kind (scale, turn or move) of a node: each is a fixed number or the offset of a track.
std::vector<float> ReadNodeValues(FileView& file, std::size_t node, std::size_t& position, std::uint32_t code, int firstFixedBit,
    std::size_t count, std::uint32_t keyFormat)
{
    std::vector<float> values;
    for (std::size_t axis = 0; axis < count; ++axis)
    {
        bool isFixed = (code & (1u << (firstFixedBit + axis))) != 0;
        if (isFixed)
            values.push_back(file.Float(position));
        else
            values.push_back(FirstKeyValue(file, node + file.Int32(position), keyFormat));
        position += sizeof(std::uint32_t);
    }
    return values;
}

Formats::Vector3 ToVector(const std::vector<float>& values)
{
    if (values.size() == 1)
        return {values[0], values[0], values[0]};
    return {values[0], values[1], values[2]};
}

std::map<std::string, BoneTransform> ReadStandingPose(FileView& file, std::size_t animationOffset)
{
    std::map<std::string, BoneTransform> pose;
    for (const GroupEntry& entry : ReadGroup(file, animationOffset + file.Int32(animationOffset + AnimationNodesPosition)))
    {
        std::size_t node = entry.offset;
        std::uint32_t code = file.Uint32(node + NodeCodePosition);
        std::size_t position = node + NodeValuesPosition;

        BoneTransform transform;
        transform.undoesParentScale = (code & UndoesParentScaleFlag) != 0;

        if ((code & (1u << HasScaleBit)) != 0)
        {
            std::size_t count = AxisCount;
            if ((code & UniformScaleFlag) != 0)
                count = 1;
            transform.scale = ToVector(ReadNodeValues(file, node, position, code, FirstFixedScaleBit, count, FloatKeys));
        }
        if ((code & (1u << HasRotationBit)) != 0)
        {
            std::uint32_t keyFormat = (code >> RotationFormatShift) & RotationFormatMask;
            transform.rotationDegrees = ToVector(ReadNodeValues(file, node, position, code, FirstFixedRotationBit, AxisCount, keyFormat));
        }
        if ((code & (1u << HasTranslationBit)) != 0)
            transform.translation = ToVector(ReadNodeValues(file, node, position, code, FirstFixedTranslationBit, AxisCount, FloatKeys));

        pose[entry.name] = transform;
    }
    return pose;
}

Formats::RgbaImage ReadTexture(FileView& file, std::size_t texture)
{
    std::uint16_t width = file.Uint16(texture + TextureWidthPosition);
    std::uint16_t height = file.Uint16(texture + TextureHeightPosition);
    std::optional<Formats::TextureFormat> format = Formats::TextureFormatFromNumber(file.Uint32(texture + TextureFormatPosition));
    if (!format || Formats::IsPaletteFormat(*format))
        file.Fail("the environment texture has a format that is not expected");

    std::size_t data = texture + file.Uint32(texture + TextureDataPosition);
    Formats::ByteView pixels = file.Bytes(data, Formats::EncodedImageSize(*format, width, height));
    return Formats::DecodeTexture(*format, width, height, pixels, std::nullopt);
}

}

MiiBodyFile ReadMiiBodyFile(Formats::ByteView brres)
{
    FileView file(brres);
    std::size_t rootGroup = file.Uint16(RootOffsetPosition) + RootGroupInsideRoot;

    std::vector<GroupEntry> models = ReadGroup(file, FindInGroup(file, rootGroup, ModelGroupName));
    if (models.empty())
        file.Fail("there is no model");
    std::size_t modelOffset = models.front().offset;

    MiiBodyFile body;
    body.bones = ReadBones(file, modelOffset);
    ReadParts(file, modelOffset, body);

    std::size_t animation = FindInGroup(file, FindInGroup(file, rootGroup, AnimationGroupName), StandingAnimationName);
    body.standingPose = ReadStandingPose(file, animation);

    std::size_t texture = FindInGroup(file, FindInGroup(file, rootGroup, TextureGroupName), EnvironmentTextureName);
    body.environmentTexture = ReadTexture(file, texture);
    return body;
}

MiiBodyFiles ReadMiiBodyFiles(Formats::ByteView characterArchive)
{
    Formats::Bytes unpacked = Formats::DecompressYaz0(characterArchive);
    Formats::U8Archive archive = Formats::ReadU8Archive(unpacked);

    std::optional<Formats::ByteView> male = archive.FindFile(MaleBodyFileName);
    std::optional<Formats::ByteView> female = archive.FindFile(FemaleBodyFileName);
    if (!male || !female)
        throw Formats::FormatError("Mii bodies: the character archive has no maleBody.brres or femaleBody.brres");

    return {ReadMiiBodyFile(*male), ReadMiiBodyFile(*female)};
}

}
