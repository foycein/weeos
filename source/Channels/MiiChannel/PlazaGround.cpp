#include "Channels/MiiChannel/PlazaGround.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/ResourceFile.h"
#include "Formats/TextureDecoder.h"
#include "Formats/TextureFormat.h"

#include <array>
#include <format>

namespace Channels
{

namespace
{

constexpr std::string_view DataDescription = "Ground of the Mii Channel";
constexpr std::string_view ModelGroup = "3DModels(NW4R)";
constexpr std::string_view TextureGroup = "Textures(NW4R)";
constexpr std::string_view PaletteGroup = "Palettes(NW4R)";

// The names of a ground in its file; the road's picture has no palette.
struct GroundNames
{
    std::string_view model;
    std::string_view texture;
    std::string_view palette;
};

constexpr GroundNames PlazaGroundNames = {"graund_model", "ground", "graund_pl"};
constexpr GroundNames ParadeRoadNames = {"road_model", "road", ""};

// The model (MDL0, version 8) lists the index groups of its parts at 0x10.
constexpr std::size_t ModelSectionsPosition = 0x10;
constexpr std::size_t PositionSection = 2;
constexpr std::size_t ColorSection = 4;
constexpr std::size_t TextureCoordinateSection = 5;
constexpr std::size_t ObjectSection = 8;

// An array of the model: where its numbers start, their format, their fraction bits, the size of one entry and the count.
constexpr std::size_t ArrayDataPosition = 0x08;
constexpr std::size_t ArrayFormatPosition = 0x18;
constexpr std::size_t ArrayShiftPosition = 0x1C;
constexpr std::size_t ArrayStridePosition = 0x1D;
constexpr std::size_t ArrayCountPosition = 0x1E;

// A color array keeps the size of an entry where the other arrays keep their fraction bits.
constexpr std::size_t ColorStridePosition = 0x1C;
constexpr std::uint32_t FloatFormat = 4;
constexpr std::uint32_t SignedShortFormat = 3;
constexpr std::uint32_t UnsignedShortFormat = 2;
constexpr std::uint32_t Rgba8Format = 5;

// The object: which attributes its corners carry, and its list of triangles.
constexpr std::size_t ObjectDescriptorPosition = 0x0C;
constexpr std::size_t ObjectTextureDescriptorPosition = 0x10;
constexpr std::size_t ObjectListPosition = 0x24;
constexpr std::size_t ObjectListSizePosition = 0x28;
constexpr std::size_t ObjectListOffsetPosition = 0x2C;

// A corner is a one-byte index for every attribute that the descriptors name, in the order position, normal, color, texture coordinate.
// Two bits each say whether an attribute is there: 0 for not there, 2 for a one-byte index.
constexpr int PositionDescriptorBit = 9;
constexpr int NormalDescriptorBit = 11;
constexpr int ColorDescriptorBit = 13;
constexpr int TextureCoordinateDescriptorBit = 0;
constexpr std::uint32_t AttributeDescriptorMask = 3;
constexpr std::uint32_t AttributeAbsent = 0;
constexpr std::uint32_t AttributeByteIndex = 2;
constexpr std::size_t IndexSize = 1;

constexpr std::uint8_t NoOperation = 0x00;
constexpr std::uint8_t TrianglesCommand = 0x90;
constexpr std::uint8_t TriangleStripCommand = 0x98;
constexpr std::uint8_t CommandMask = 0xF8;

// A command of the list is one byte, then the count of its corners in 16 bits, then the corners.
constexpr std::size_t CommandSize = 1;
constexpr std::size_t CornerCountSize = 2;

constexpr std::size_t TextureDataPosition = 0x10;
constexpr std::size_t TextureWidthPosition = 0x1C;
constexpr std::size_t TextureHeightPosition = 0x1E;
constexpr std::size_t TextureFormatPosition = 0x20;
constexpr std::size_t PaletteDataPosition = 0x10;
constexpr std::size_t PaletteFormatPosition = 0x18;
constexpr std::size_t PaletteCountPosition = 0x1C;

// The texture matrix of mt_ground: the paving six times across, moved by half a picture; mirrored at the edges (wrap mode 2).
constexpr float PavingRepeat = 6.0f;
constexpr float PavingShift = 0.5f;
constexpr Graphics::TextureMatrix PlazaPavingMatrix = {{{{PavingRepeat, 0.0f, PavingShift}, {0.0f, PavingRepeat, 0.0f}}}};

// The texture matrix of mt_road: the road picture seven times across, turned by 45 degrees (7 x cos 45 = 7 x sin 45 = 4.9497);
// repeated at the edges.
constexpr float RoadScaledTurn = 4.9497475f;
constexpr Graphics::TextureMatrix RoadMatrix = {{{{RoadScaledTurn, -RoadScaledTurn, 0.0f}, {RoadScaledTurn, RoadScaledTurn, 0.0f}}}};

// The second texture of mt_road, alpha_tex, is scaled by 2 about the middle of the road and clamped at its edges (the SRT of the
// material: scale 2, no turn; CHN-84), so the road shows in the middle half of road_model and fades to white at its sides.
constexpr std::string_view SideFadeTexture = "alpha_tex";
constexpr float SideFadeScale = 2.0f;
constexpr float SideFadeShift = 0.5f - SideFadeScale * 0.5f;
constexpr Graphics::TextureMatrix SideFadeMatrix = {{{{SideFadeScale, 0.0f, SideFadeShift}, {0.0f, SideFadeScale, SideFadeShift}}}};

constexpr Formats::Rgba8Color White = {255, 255, 255, 255};

// The sizes of the numbers of an array entry.
constexpr std::size_t FloatSize = sizeof(float);
constexpr std::size_t ShortSize = sizeof(std::uint16_t);

class FileView
{
public:
    explicit FileView(Formats::ByteView bytes)
        : reader(bytes, DataDescription)
        , fileBytes(bytes)
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

    std::uint32_t Uint32(std::size_t offset)
    {
        reader.MoveTo(offset);
        return reader.ReadUint32();
    }

    // An offset of the file is signed and counts from a place of its own.
    std::size_t Offset(std::size_t from, std::size_t position)
    {
        auto signedOffset = static_cast<std::int32_t>(Uint32(position));
        return from + static_cast<std::size_t>(signedOffset);
    }

    float Float(std::size_t offset)
    {
        reader.MoveTo(offset);
        return reader.ReadFloat();
    }

    Formats::ByteView Bytes(std::size_t offset, std::size_t size)
    {
        reader.MoveTo(offset);
        return reader.ReadBytes(size);
    }

    Formats::ByteView All() const
    {
        return fileBytes;
    }

    [[noreturn]] void Fail(std::string_view problem) const
    {
        reader.Fail(problem);
    }

private:
    Formats::ByteReader reader;
    Formats::ByteView fileBytes;
};

std::size_t FirstEntryOfSection(FileView& file, std::size_t model, std::size_t section)
{
    std::size_t group = file.Offset(model, model + ModelSectionsPosition + section * sizeof(std::uint32_t));
    std::vector<Formats::ResourceEntry> entries = Formats::ReadIndexGroup(file.All(), group);
    if (entries.empty())
        file.Fail(std::format("the model has nothing in its part {}", section));
    return entries.front().offset;
}

// Positions are floats, or signed 16-bit numbers with as many fraction bits as the array says.
std::vector<Formats::Vector3> ReadPositions(FileView& file, std::size_t array)
{
    std::uint32_t format = file.Uint32(array + ArrayFormatPosition);
    if (format != FloatFormat && format != SignedShortFormat)
        file.Fail("the positions are neither floats nor signed 16-bit numbers");

    std::size_t data = file.Offset(array, array + ArrayDataPosition);
    std::uint8_t stride = file.Byte(array + ArrayStridePosition);
    auto unit = static_cast<float>(1u << file.Byte(array + ArrayShiftPosition));
    std::vector<Formats::Vector3> positions;
    for (std::uint16_t number = 0; number < file.Uint16(array + ArrayCountPosition); ++number)
    {
        std::size_t entry = data + std::size_t{number} * stride;
        if (format == FloatFormat)
        {
            positions.push_back({file.Float(entry), file.Float(entry + FloatSize), file.Float(entry + 2 * FloatSize)});
            continue;
        }

        auto x = static_cast<std::int16_t>(file.Uint16(entry));
        auto y = static_cast<std::int16_t>(file.Uint16(entry + ShortSize));
        auto z = static_cast<std::int16_t>(file.Uint16(entry + 2 * ShortSize));
        positions.push_back({x / unit, y / unit, z / unit});
    }
    return positions;
}

std::vector<Formats::Rgba8Color> ReadColors(FileView& file, std::size_t array)
{
    if (file.Uint32(array + ArrayFormatPosition) != Rgba8Format)
        file.Fail("the corner colors are not RGBA8");

    std::size_t data = file.Offset(array, array + ArrayDataPosition);
    std::uint8_t stride = file.Byte(array + ColorStridePosition);
    std::vector<Formats::Rgba8Color> colors;
    for (std::uint16_t number = 0; number < file.Uint16(array + ArrayCountPosition); ++number)
    {
        std::size_t entry = data + std::size_t{number} * stride;
        colors.push_back({file.Byte(entry), file.Byte(entry + 1), file.Byte(entry + 2), file.Byte(entry + 3)});
    }
    return colors;
}

std::vector<Formats::Vector2> ReadTextureCoordinates(FileView& file, std::size_t array)
{
    if (file.Uint32(array + ArrayFormatPosition) != UnsignedShortFormat)
        file.Fail("the texture coordinates are not unsigned 16-bit numbers");

    std::size_t data = file.Offset(array, array + ArrayDataPosition);
    auto unit = static_cast<float>(1u << file.Byte(array + ArrayShiftPosition));
    std::uint8_t stride = file.Byte(array + ArrayStridePosition);
    std::vector<Formats::Vector2> coordinates;
    for (std::uint16_t number = 0; number < file.Uint16(array + ArrayCountPosition); ++number)
    {
        std::size_t entry = data + std::size_t{number} * stride;
        coordinates.push_back({file.Uint16(entry) / unit, file.Uint16(entry + ShortSize) / unit});
    }
    return coordinates;
}

struct GroundArrays
{
    std::vector<Formats::Vector3> positions;
    std::vector<Formats::Rgba8Color> colors;
    std::vector<Formats::Vector2> textureCoordinates;
};

// Where the bytes of a corner are: the position, the normal (whose value the ground does not need), the color, the texture coordinate.
struct CornerLayout
{
    std::size_t positionByte = 0;
    std::size_t colorByte = 0;
    std::size_t textureCoordinateByte = 0;
    std::size_t size = 0;
};

bool HasAttribute(FileView& file, std::uint32_t descriptor, int bit)
{
    std::uint32_t kind = (descriptor >> bit) & AttributeDescriptorMask;
    if (kind != AttributeAbsent && kind != AttributeByteIndex)
        file.Fail("a corner of the ground has an attribute that is not a one-byte index");
    return kind == AttributeByteIndex;
}

CornerLayout ReadCornerLayout(FileView& file, std::size_t object)
{
    std::uint32_t descriptor = file.Uint32(object + ObjectDescriptorPosition);
    std::uint32_t textureDescriptor = file.Uint32(object + ObjectTextureDescriptorPosition);
    if (!HasAttribute(file, descriptor, PositionDescriptorBit) || !HasAttribute(file, descriptor, ColorDescriptorBit)
        || !HasAttribute(file, textureDescriptor, TextureCoordinateDescriptorBit))
        file.Fail("the corners of the ground have no position, color or texture coordinate");

    bool hasNormal = HasAttribute(file, descriptor, NormalDescriptorBit);

    CornerLayout layout;
    layout.positionByte = 0;
    layout.colorByte = layout.positionByte + IndexSize;
    if (hasNormal)
        layout.colorByte += IndexSize;
    layout.textureCoordinateByte = layout.colorByte + IndexSize;
    layout.size = layout.textureCoordinateByte + IndexSize;
    return layout;
}

Graphics::MeshVertex ReadCorner(FileView& file, std::size_t corner, const CornerLayout& layout, const GroundArrays& arrays)
{
    Graphics::MeshVertex vertex;
    vertex.position = arrays.positions.at(file.Byte(corner + layout.positionByte));
    vertex.normal = {0, 1, 0};
    vertex.color = arrays.colors.at(file.Byte(corner + layout.colorByte));
    vertex.textureCoordinate = arrays.textureCoordinates.at(file.Byte(corner + layout.textureCoordinateByte));
    return vertex;
}

// A strip of n corners is n - 2 triangles; every second one is turned around so that all go around the same way.
std::vector<Graphics::MeshVertex> ReadTriangles(FileView& file, std::size_t object, const GroundArrays& arrays)
{
    CornerLayout layout = ReadCornerLayout(file, object);

    std::size_t list = file.Offset(object + ObjectListPosition, object + ObjectListOffsetPosition);
    std::size_t listEnd = list + file.Uint32(object + ObjectListSizePosition);

    std::vector<Graphics::MeshVertex> triangles;
    std::size_t position = list;
    while (position < listEnd)
    {
        std::uint8_t command = file.Byte(position);
        if (command == NoOperation)
        {
            ++position;
            continue;
        }

        std::uint8_t kind = command & CommandMask;
        if (kind != TrianglesCommand && kind != TriangleStripCommand)
            file.Fail(std::format("the list of triangles has the command {:#04x}", command));

        std::uint16_t count = file.Uint16(position + CommandSize);
        std::size_t first = position + CommandSize + CornerCountSize;
        std::vector<Graphics::MeshVertex> corners;
        for (std::uint16_t number = 0; number < count; ++number)
            corners.push_back(ReadCorner(file, first + std::size_t{number} * layout.size, layout, arrays));
        position = first + std::size_t{count} * layout.size;

        if (kind == TrianglesCommand)
        {
            triangles.insert(triangles.end(), corners.begin(), corners.end());
            continue;
        }

        for (std::size_t corner = 0; corner + 2 < corners.size(); ++corner)
        {
            bool isTurned = corner % 2 == 1;
            triangles.push_back(isTurned ? corners[corner + 1] : corners[corner]);
            triangles.push_back(isTurned ? corners[corner] : corners[corner + 1]);
            triangles.push_back(corners[corner + 2]);
        }
    }
    return triangles;
}

std::optional<Formats::TexturePalette> ReadPalette(FileView& file, std::string_view paletteName)
{
    if (paletteName.empty())
        return std::nullopt;

    std::optional<std::size_t> palette = Formats::FindResource(file.All(), PaletteGroup, paletteName);
    if (!palette)
        file.Fail("the palette of the picture is missing");

    std::optional<Formats::PaletteFormat> paletteFormat = Formats::PaletteFormatFromNumber(file.Uint32(*palette + PaletteFormatPosition));
    if (!paletteFormat)
        file.Fail("the palette has a format that is not known");

    Formats::TexturePalette colors;
    colors.format = *paletteFormat;
    colors.entryCount = file.Uint16(*palette + PaletteCountPosition);
    Formats::ByteView entries = file.Bytes(file.Offset(*palette, *palette + PaletteDataPosition), colors.entryCount * sizeof(std::uint16_t));
    colors.entries.assign(entries.begin(), entries.end());
    return colors;
}

Formats::RgbaImage ReadPicture(FileView& file, std::string_view textureName, std::string_view paletteName)
{
    std::optional<std::size_t> texture = Formats::FindResource(file.All(), TextureGroup, textureName);
    if (!texture)
        file.Fail("the picture of the ground is missing");

    std::uint16_t width = file.Uint16(*texture + TextureWidthPosition);
    std::uint16_t height = file.Uint16(*texture + TextureHeightPosition);
    std::optional<Formats::TextureFormat> format = Formats::TextureFormatFromNumber(file.Uint32(*texture + TextureFormatPosition));
    if (!format)
        file.Fail("the picture of the ground has a format that is not known");

    std::optional<Formats::TexturePalette> palette = ReadPalette(file, paletteName);
    std::size_t data = file.Offset(*texture, *texture + TextureDataPosition);
    Formats::ByteView pixels = file.Bytes(data, Formats::EncodedImageSize(*format, width, height));
    return Formats::DecodeTexture(*format, width, height, pixels, palette);
}

PlazaGroundModel ReadGroundModel(Formats::ByteView brres, const GroundNames& names)
{
    FileView file(brres);
    std::optional<std::size_t> model = Formats::FindResource(brres, ModelGroup, names.model);
    if (!model)
        file.Fail(std::format("there is no model {}", names.model));

    GroundArrays arrays;
    arrays.positions = ReadPositions(file, FirstEntryOfSection(file, *model, PositionSection));
    arrays.colors = ReadColors(file, FirstEntryOfSection(file, *model, ColorSection));
    arrays.textureCoordinates = ReadTextureCoordinates(file, FirstEntryOfSection(file, *model, TextureCoordinateSection));

    PlazaGroundModel ground;
    ground.triangleCorners = ReadTriangles(file, FirstEntryOfSection(file, *model, ObjectSection), arrays);
    ground.paving = ReadPicture(file, names.texture, names.palette);
    return ground;
}

}

PlazaGroundModel ReadPlazaGroundModel(Formats::ByteView brres)
{
    PlazaGroundModel ground = ReadGroundModel(brres, PlazaGroundNames);
    ground.pavingMatrix = PlazaPavingMatrix;
    ground.pavingWrap = Graphics::WrapMode::Mirror;
    ground.fade = WhiteFade::WhereAlphaIsHigh;
    return ground;
}

PlazaGroundModel ReadParadeRoadModel(Formats::ByteView brres)
{
    PlazaGroundModel road = ReadGroundModel(brres, ParadeRoadNames);
    FileView file(brres);
    road.sideFade = ReadPicture(file, SideFadeTexture, "");
    road.pavingMatrix = RoadMatrix;
    road.pavingWrap = Graphics::WrapMode::Repeat;
    road.fade = WhiteFade::WhereAlphaIsLow;
    return road;
}

void DrawWhiteBackground(Graphics::GraphicsDevice& device, Graphics::Canvas canvas)
{
    float halfWidth = canvas.width / 2;
    float halfHeight = canvas.height / 2;
    Graphics::Quad background;
    background.positions = {{{-halfWidth, halfHeight}, {halfWidth, halfHeight}, {halfWidth, -halfHeight}, {-halfWidth, -halfHeight}}};
    background.colors.fill(White);
    device.DrawQuad(background, Graphics::Material());
}

PlazaGround::PlazaGround(Graphics::GraphicsDevice& graphicsDevice, const PlazaGroundModel& model)
    : device(graphicsDevice)
    , mesh(graphicsDevice.CreateMesh(model.triangleCorners))
    , pavingMatrix(model.pavingMatrix)
    , fade(model.fade)
{
    Graphics::TextureDescription description;
    description.width = model.paving.width;
    description.height = model.paving.height;
    description.pixels = model.paving.pixels;
    description.horizontalWrap = model.pavingWrap;
    description.verticalWrap = model.pavingWrap;
    paving = device.CreateTexture(description);

    if (!model.sideFade)
        return;

    Graphics::TextureDescription fadeDescription;
    fadeDescription.width = model.sideFade->width;
    fadeDescription.height = model.sideFade->height;
    fadeDescription.pixels = model.sideFade->pixels;
    fadeDescription.horizontalWrap = Graphics::WrapMode::Clamp;
    fadeDescription.verticalWrap = Graphics::WrapMode::Clamp;
    sideFade = device.CreateTexture(fadeDescription);
}

PlazaGround::~PlazaGround()
{
    device.DestroyMesh(mesh);
    device.DestroyTexture(paving);
    if (sideFade)
        device.DestroyTexture(*sideFade);
}

void PlazaGround::Draw(const Graphics::Matrix4& modelView, const Graphics::Matrix4& projection) const
{
    Graphics::TevStage stage;
    stage.textureIndex = 0;
    stage.usesRasterColor = true;
    stage.color.inputA = Graphics::TevColorInput::TextureColor;
    stage.color.inputB = Graphics::TevColorInput::One;
    if (fade == WhiteFade::WhereAlphaIsLow)
    {
        stage.color.inputA = Graphics::TevColorInput::One;
        stage.color.inputB = Graphics::TevColorInput::TextureColor;
    }
    stage.color.inputC = Graphics::TevColorInput::RasterAlpha;
    stage.alpha.inputD = Graphics::TevAlphaInput::TextureAlpha;

    Graphics::Material material;
    material.blendMode = Graphics::BlendMode::Replace;
    material.textures = {{paving, pavingMatrix}};
    material.stages = {stage};

    // The sides fade into white: white x (1 - fade) + the first stage x fade (the second stage of mt_road).
    if (sideFade)
    {
        Graphics::TevStage fadeStage;
        fadeStage.textureIndex = 1;
        fadeStage.usesRasterColor = false;
        fadeStage.color.inputA = Graphics::TevColorInput::One;
        fadeStage.color.inputB = Graphics::TevColorInput::PreviousColor;
        fadeStage.color.inputC = Graphics::TevColorInput::TextureColor;
        fadeStage.alpha.inputD = Graphics::TevAlphaInput::PreviousAlpha;
        material.textures.push_back({*sideFade, SideFadeMatrix});
        material.stages.push_back(fadeStage);
    }

    Graphics::MeshPlacement placement;
    placement.modelView = modelView;
    placement.projection = projection;
    placement.usesCornerColors = true;
    placement.culling = Graphics::FaceCulling::Back;
    device.DrawMesh(mesh, placement, material);
}

}
