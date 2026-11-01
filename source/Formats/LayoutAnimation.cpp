#include "Formats/LayoutAnimation.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/SectionedFile.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view DataDescription = "Layout animation";
// The System Menu and most channels write version 8; the Photo Channel 1.1 writes version 10, whose sections it uses are the same.
constexpr std::uint16_t SupportedVersion = 8;
constexpr std::uint16_t PhotoChannelVersion = 10;

constexpr std::size_t ObjectNameSize = 20;
constexpr std::size_t TagKindSize = 4;
constexpr Byte PaneObjectKind = 0;
constexpr Byte MaterialObjectKind = 1;

constexpr std::size_t UnusedAfterLoopSize = 1;
constexpr std::size_t UnusedAfterObjectKindSize = 2;
constexpr std::size_t UnusedAfterTargetCountSize = 3;
constexpr std::size_t UnusedAfterTargetKindSize = 1;
constexpr std::size_t UnusedAfterKeyCountSize = 2;
constexpr std::size_t UnusedAfterStepValueSize = 2;

struct TagNameAndKind
{
    std::string_view name;
    AnimationTagKind kind;
};

constexpr TagNameAndKind TagKinds[] = {
    {"RLPA", AnimationTagKind::PaneTransform},
    {"RLTS", AnimationTagKind::TextureTransform},
    {"RLVI", AnimationTagKind::Visibility},
    {"RLVC", AnimationTagKind::VertexColor},
    {"RLMC", AnimationTagKind::MaterialColor},
    {"RLTP", AnimationTagKind::TexturePattern},
};

std::vector<std::uint32_t> ReadOffsets(ByteReader& reader, std::size_t count)
{
    std::vector<std::uint32_t> offsets;
    for (std::size_t number = 0; number < count; ++number)
        offsets.push_back(reader.ReadUint32());
    return offsets;
}

AnimationTagKind ReadTagKind(ByteReader& reader)
{
    std::string name = reader.ReadText(TagKindSize);

    for (const TagNameAndKind& tagKind : TagKinds)
    {
        if (tagKind.name == name)
            return tagKind.kind;
    }

    reader.Fail(std::format("the animation tag {} is not known", name));
}

KeyFrameKind ReadKeyFrameKind(ByteReader& reader)
{
    Byte number = reader.ReadByte();
    if (number != static_cast<Byte>(KeyFrameKind::Step) && number != static_cast<Byte>(KeyFrameKind::Hermite))
        reader.Fail(std::format("the key frame type {} is not known", number));

    return static_cast<KeyFrameKind>(number);
}

KeyFrame ReadKeyFrame(ByteReader& reader, KeyFrameKind kind)
{
    KeyFrame keyFrame;
    keyFrame.frame = reader.ReadFloat();

    if (kind == KeyFrameKind::Step)
    {
        keyFrame.value = reader.ReadUint16();
        keyFrame.slope = 0.0f;
        reader.Skip(UnusedAfterStepValueSize);
        return keyFrame;
    }

    keyFrame.value = reader.ReadFloat();
    keyFrame.slope = reader.ReadFloat();
    return keyFrame;
}

AnimationTarget ReadTarget(ByteReader& reader)
{
    std::size_t targetStart = reader.Position();

    AnimationTarget target;
    target.index = reader.ReadByte();
    target.target = reader.ReadByte();
    target.keyFrameKind = ReadKeyFrameKind(reader);
    reader.Skip(UnusedAfterTargetKindSize);
    std::uint16_t keyFrameCount = reader.ReadUint16();
    reader.Skip(UnusedAfterKeyCountSize);
    std::uint32_t keyFramesOffset = reader.ReadUint32();

    reader.MoveTo(targetStart + keyFramesOffset);
    for (std::uint16_t keyFrameNumber = 0; keyFrameNumber < keyFrameCount; ++keyFrameNumber)
        target.keyFrames.push_back(ReadKeyFrame(reader, target.keyFrameKind));

    return target;
}

AnimationTag ReadTag(ByteReader& reader)
{
    std::size_t tagStart = reader.Position();

    AnimationTag tag;
    tag.kind = ReadTagKind(reader);
    Byte targetCount = reader.ReadByte();
    reader.Skip(UnusedAfterTargetCountSize);
    std::vector<std::uint32_t> targetOffsets = ReadOffsets(reader, targetCount);

    for (std::uint32_t targetOffset : targetOffsets)
    {
        reader.MoveTo(tagStart + targetOffset);
        tag.targets.push_back(ReadTarget(reader));
    }

    return tag;
}

AnimatedObject ReadObject(ByteReader& reader)
{
    std::size_t objectStart = reader.Position();

    AnimatedObject object;
    object.name = reader.ReadText(ObjectNameSize);
    Byte tagCount = reader.ReadByte();

    Byte kind = reader.ReadByte();
    if (kind != PaneObjectKind && kind != MaterialObjectKind)
        reader.Fail(std::format("the animated object kind {} is not known", kind));
    object.isMaterial = kind == MaterialObjectKind;

    reader.Skip(UnusedAfterObjectKindSize);
    std::vector<std::uint32_t> tagOffsets = ReadOffsets(reader, tagCount);

    for (std::uint32_t tagOffset : tagOffsets)
    {
        reader.MoveTo(objectStart + tagOffset);
        object.tags.push_back(ReadTag(reader));
    }

    return object;
}

std::vector<std::string> ReadTextureNames(ByteReader& reader, std::size_t count, const Section& section)
{
    std::size_t listStart = reader.Position();
    std::vector<std::uint32_t> nameOffsets = ReadOffsets(reader, count);

    std::vector<std::string> names;
    for (std::uint32_t nameOffset : nameOffsets)
    {
        ByteReader nameReader(section.bytes, DataDescription);
        nameReader.MoveTo(listStart + nameOffset);
        names.push_back(nameReader.ReadTextUntilZero());
    }
    return names;
}

LayoutAnimation ReadAnimationSection(const Section& section)
{
    ByteReader reader = OpenSection(section, DataDescription);

    LayoutAnimation animation;
    animation.frameCount = reader.ReadUint16();
    animation.isLooping = reader.ReadByte() != 0;
    reader.Skip(UnusedAfterLoopSize);
    std::uint16_t textureCount = reader.ReadUint16();
    std::uint16_t objectCount = reader.ReadUint16();
    std::uint32_t objectOffsetsOffset = reader.ReadUint32();

    animation.textureNames = ReadTextureNames(reader, textureCount, section);

    reader.MoveTo(objectOffsetsOffset);
    std::vector<std::uint32_t> objectOffsets = ReadOffsets(reader, objectCount);

    for (std::uint32_t objectOffset : objectOffsets)
    {
        reader.MoveTo(objectOffset);
        animation.objects.push_back(ReadObject(reader));
    }

    return animation;
}

}

LayoutAnimation ReadLayoutAnimation(ByteView bytes)
{
    SectionedFile file = ReadSectionedFile(bytes, "RLAN", DataDescription);
    if (file.version != SupportedVersion && file.version != PhotoChannelVersion)
    {
        throw FormatError(std::format("{}: the version {} is not supported, only versions {} and {} are known", DataDescription,
            file.version, SupportedVersion, PhotoChannelVersion));
    }

    SectionCursor cursor(file.sections, DataDescription);
    LayoutAnimation animation = ReadAnimationSection(cursor.Take("pai1"));

    if (!cursor.IsAtEnd())
        throw FormatError(std::format("{}: the section {} comes at a place where no section is expected", DataDescription, cursor.Take().tag));

    return animation;
}

}
