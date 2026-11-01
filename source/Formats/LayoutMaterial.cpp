#include "Formats/LayoutMaterial.h"

#include "Formats/BitFields.h"
#include "Formats/ByteReader.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::size_t NameSize = 20;
constexpr std::size_t MaterialListUnusedSize = 2;
constexpr std::size_t UnusedAfterGeneratorSize = 1;
constexpr std::size_t UnusedAfterChannelControlSize = 2;

// The bits of the flags of a material.
constexpr int TextureCountBit = 0;
constexpr int TextureCountBitCount = 4;
constexpr int TextureTransformCountBit = 4;
constexpr int TextureTransformCountBitCount = 4;
constexpr int TextureCoordinateGeneratorCountBit = 8;
constexpr int TextureCoordinateGeneratorCountBitCount = 4;
constexpr int HasTevSwapTableBit = 12;
constexpr int IndirectMatrixCountBit = 13;
constexpr int IndirectMatrixCountBitCount = 2;
constexpr int IndirectStageCountBit = 15;
constexpr int IndirectStageCountBitCount = 3;
constexpr int TevStageCountBit = 18;
constexpr int TevStageCountBitCount = 5;
constexpr int HasAlphaCompareBit = 23;
constexpr int HasBlendModeBit = 24;
constexpr int HasChannelControlBit = 25;
constexpr int HasMaterialColorBit = 27;
constexpr std::uint32_t KnownFlagBits = 0x0BFFFFFF;

// The wrap modes of a texture are stored in the low bits of two bytes: the horizontal one in the first byte
// and the vertical one in the second byte. The filters are stored above them.
constexpr int WrapBitCount = 2;
constexpr int MinificationFilterBit = 2;
constexpr int MinificationFilterBitCount = 3;
constexpr int MagnificationFilterBit = 2;
constexpr int MagnificationFilterBitCount = 2;
constexpr unsigned HighestWrapNumber = 2;

// The two nibbles of the first byte of an alpha compare.
constexpr int SecondConditionBit = 4;
constexpr int ConditionBitCount = 4;

SignedColor ReadSignedColor(ByteReader& reader)
{
    SignedColor color;
    color.red = reader.ReadInt16();
    color.green = reader.ReadInt16();
    color.blue = reader.ReadInt16();
    color.alpha = reader.ReadInt16();
    return color;
}

TextureWrap ToTextureWrap(ByteReader& reader, unsigned number)
{
    if (number > HighestWrapNumber)
        reader.Fail(std::format("the wrap mode {} is not known", number));

    return static_cast<TextureWrap>(number);
}

MaterialTexture ReadMaterialTexture(ByteReader& reader)
{
    MaterialTexture texture;
    texture.textureIndex = reader.ReadUint16();

    Byte horizontalByte = reader.ReadByte();
    Byte verticalByte = reader.ReadByte();
    texture.horizontalWrap = ToTextureWrap(reader, ExtractBits(horizontalByte, 0, WrapBitCount));
    texture.verticalWrap = ToTextureWrap(reader, ExtractBits(verticalByte, 0, WrapBitCount));
    texture.minificationFilter = static_cast<std::uint8_t>(ExtractBits(horizontalByte, MinificationFilterBit, MinificationFilterBitCount));
    texture.magnificationFilter = static_cast<std::uint8_t>(ExtractBits(verticalByte, MagnificationFilterBit, MagnificationFilterBitCount));
    return texture;
}

TextureTransform ReadTextureTransform(ByteReader& reader)
{
    TextureTransform transform;
    transform.translation = ReadVector2(reader);
    transform.rotation = reader.ReadFloat();
    transform.scale = ReadVector2(reader);
    return transform;
}

TextureCoordinateGenerator ReadTextureCoordinateGenerator(ByteReader& reader)
{
    TextureCoordinateGenerator generator;
    generator.matrixType = reader.ReadByte();
    generator.source = reader.ReadByte();
    generator.matrixSource = reader.ReadByte();
    reader.Skip(UnusedAfterGeneratorSize);
    return generator;
}

ColorSource ReadColorSource(ByteReader& reader)
{
    Byte number = reader.ReadByte();
    if (number > static_cast<Byte>(ColorSource::Vertex))
        reader.Fail(std::format("the color source {} is not known", number));

    return static_cast<ColorSource>(number);
}

ChannelControl ReadChannelControl(ByteReader& reader)
{
    ChannelControl control;
    control.colorSource = ReadColorSource(reader);
    control.alphaSource = ReadColorSource(reader);
    reader.Skip(UnusedAfterChannelControlSize);
    return control;
}

AlphaCompare ReadAlphaCompare(ByteReader& reader)
{
    Byte conditions = reader.ReadByte();

    AlphaCompare compare;
    compare.firstCondition = static_cast<std::uint8_t>(ExtractBits(conditions, 0, ConditionBitCount));
    compare.secondCondition = static_cast<std::uint8_t>(ExtractBits(conditions, SecondConditionBit, ConditionBitCount));
    compare.operation = reader.ReadByte();
    compare.firstReference = reader.ReadByte();
    compare.secondReference = reader.ReadByte();
    return compare;
}

BlendMode ReadBlendMode(ByteReader& reader)
{
    BlendMode mode;
    mode.function = reader.ReadByte();
    mode.sourceFactor = reader.ReadByte();
    mode.destinationFactor = reader.ReadByte();
    mode.logicOperation = reader.ReadByte();
    return mode;
}

unsigned CountIn(std::uint32_t flags, int lowestBit, int bitCount)
{
    return ExtractBits(flags, lowestBit, bitCount);
}

bool IsSet(std::uint32_t flags, int bit)
{
    return ExtractBits(flags, bit, 1) != 0;
}

void RequireSupportedFlags(ByteReader& reader, std::uint32_t flags, const std::string& materialName)
{
    if ((flags & ~KnownFlagBits) != 0)
        reader.Fail(std::format("the material {} has the unknown flags {:#x}", materialName, flags & ~KnownFlagBits));
}

Material ReadMaterial(ByteReader& reader)
{
    Material material;
    material.name = reader.ReadText(NameSize);
    material.foreColor = ReadSignedColor(reader);
    material.backColor = ReadSignedColor(reader);
    material.colorRegister3 = ReadSignedColor(reader);
    for (Rgba8Color& color : material.constantColors)
        color = ReadRgba8Color(reader);

    std::uint32_t flags = reader.ReadUint32();
    RequireSupportedFlags(reader, flags, material.name);

    for (unsigned count = CountIn(flags, TextureCountBit, TextureCountBitCount); count > 0; --count)
        material.textures.push_back(ReadMaterialTexture(reader));

    for (unsigned count = CountIn(flags, TextureTransformCountBit, TextureTransformCountBitCount); count > 0; --count)
        material.textureTransforms.push_back(ReadTextureTransform(reader));

    for (unsigned count = CountIn(flags, TextureCoordinateGeneratorCountBit, TextureCoordinateGeneratorCountBitCount); count > 0; --count)
        material.textureCoordinateGenerators.push_back(ReadTextureCoordinateGenerator(reader));

    if (IsSet(flags, HasChannelControlBit))
        material.channelControl = ReadChannelControl(reader);

    if (IsSet(flags, HasMaterialColorBit))
        material.materialColor = ReadRgba8Color(reader);

    if (IsSet(flags, HasTevSwapTableBit))
        material.tevSwapTable = reader.ReadByteArray<TevSwapTableSize>();

    for (unsigned count = CountIn(flags, IndirectMatrixCountBit, IndirectMatrixCountBitCount); count > 0; --count)
        material.indirectMatrices.push_back(reader.ReadByteArray<IndirectMatrixSize>());

    for (unsigned count = CountIn(flags, IndirectStageCountBit, IndirectStageCountBitCount); count > 0; --count)
        material.indirectStages.push_back(reader.ReadByteArray<IndirectStageSize>());

    for (unsigned count = CountIn(flags, TevStageCountBit, TevStageCountBitCount); count > 0; --count)
        material.tevStages.push_back(reader.ReadByteArray<TevStageSize>());

    if (IsSet(flags, HasAlphaCompareBit))
        material.alphaCompare = ReadAlphaCompare(reader);

    if (IsSet(flags, HasBlendModeBit))
        material.blendMode = ReadBlendMode(reader);

    return material;
}

}

std::vector<Material> ReadMaterials(const Section& section)
{
    ByteReader reader = OpenSection(section, "Layout");

    std::uint16_t materialCount = reader.ReadUint16();
    reader.Skip(MaterialListUnusedSize);

    std::vector<std::uint32_t> offsets;
    for (std::uint16_t materialNumber = 0; materialNumber < materialCount; ++materialNumber)
        offsets.push_back(reader.ReadUint32());

    std::vector<Material> materials;
    for (std::size_t materialNumber = 0; materialNumber < offsets.size(); ++materialNumber)
    {
        reader.MoveTo(offsets[materialNumber]);
        materials.push_back(ReadMaterial(reader));

        bool isLast = materialNumber + 1 == offsets.size();
        std::size_t endOffset = isLast ? section.bytes.size() : offsets[materialNumber + 1];
        if (reader.Position() != endOffset)
        {
            reader.Fail(std::format("the material {} ends at {:#x}, but the next one starts at {:#x}",
                materials.back().name, reader.Position(), endOffset));
        }
    }

    return materials;
}

}
