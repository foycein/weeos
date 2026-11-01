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

constexpr std::string_view DataDescription = "Ground of the Mii Plaza";
constexpr std::string_view ModelGroup = "3DModels(NW4R)";
constexpr std::string_view TextureGroup = "Textures(NW4R)";
constexpr std::string_view PaletteGroup = "Palettes(NW4R)";
constexpr std::string_view ModelName = "graund_model";
constexpr std::string_view TextureName = "ground";
constexpr std::string_view PaletteName = "graund_pl";

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
constexpr std::uint32_t UnsignedShortFormat = 2;
constexpr std::uint32_t Rgba8Format = 5;

// The object: which attributes its corners carry, and its list of triangles.
constexpr std::size_t ObjectDescriptorPosition = 0x0C;
constexpr std::size_t ObjectTextureDescriptorPosition = 0x10;
constexpr std::size_t ObjectListPosition = 0x24;
constexpr std::size_t ObjectListSizePosition = 0x28;
constexpr std::size_t ObjectListOffsetPosition = 0x2C;

// Every corner of the ground is four one-byte indexes: position, normal, color and texture coordinate.
constexpr std::uint32_t GroundCornerDescriptor = 0x5400;
constexpr std::uint32_t GroundTextureDescriptor = 0x2;
constexpr std::size_t GroundCornerSize = 4;
constexpr std::size_t CornerPositionByte = 0;
constexpr std::size_t CornerColorByte = 2;
constexpr std::size_t CornerTextureCoordinateByte = 3;

constexpr std::uint8_t NoOperation = 0x00;
constexpr std::uint8_t TrianglesCommand = 0x90;
constexpr std::uint8_t TriangleStripCommand = 0x98;
constexpr std::uint8_t CommandMask = 0xF8;

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

    std::size_t Offset(std::size_t from, std::size_t position)
    {
        return from + static_cast<std::size_t>(static_cast<std::int32_t>(Uint32(position)));
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

std::vector<Formats::Vector3> ReadPositions(FileView& file, std::size_t array)
{
    if (file.Uint32(array + ArrayFormatPosition) != FloatFormat)
        file.Fail("the positions are not floats");

    std::size_t data = file.Offset(array, array + ArrayDataPosition);
    std::uint8_t stride = file.Byte(array + ArrayStridePosition);
    std::vector<Formats::Vector3> positions;
    for (std::uint16_t number = 0; number < file.Uint16(array + ArrayCountPosition); ++number)
    {
        std::size_t entry = data + std::size_t{number} * stride;
        positions.push_back({file.Float(entry), file.Float(entry + 4), file.Float(entry + 8)});
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
        coordinates.push_back({file.Uint16(entry) / unit, file.Uint16(entry + 2) / unit});
    }
    return coordinates;
}

struct GroundArrays
{
    std::vector<Formats::Vector3> positions;
    std::vector<Formats::Rgba8Color> colors;
    std::vector<Formats::Vector2> textureCoordinates;
};

Graphics::MeshVertex ReadCorner(FileView& file, std::size_t corner, const GroundArrays& arrays)
{
    Graphics::MeshVertex vertex;
    vertex.position = arrays.positions.at(file.Byte(corner + CornerPositionByte));
    vertex.normal = {0, 1, 0};
    vertex.color = arrays.colors.at(file.Byte(corner + CornerColorByte));
    vertex.textureCoordinate = arrays.textureCoordinates.at(file.Byte(corner + CornerTextureCoordinateByte));
    return vertex;
}

// A strip of n corners is n - 2 triangles; every second one is turned around so that all go around the same way.
std::vector<Graphics::MeshVertex> ReadTriangles(FileView& file, std::size_t object, const GroundArrays& arrays)
{
    if (file.Uint32(object + ObjectDescriptorPosition) != GroundCornerDescriptor
        || file.Uint32(object + ObjectTextureDescriptorPosition) != GroundTextureDescriptor)
        file.Fail("the corners of the ground are not position, normal, color and texture coordinate");

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

        std::uint16_t count = file.Uint16(position + 1);
        std::size_t first = position + 3;
        std::vector<Graphics::MeshVertex> corners;
        for (std::uint16_t number = 0; number < count; ++number)
            corners.push_back(ReadCorner(file, first + std::size_t{number} * GroundCornerSize, arrays));
        position = first + std::size_t{count} * GroundCornerSize;

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

Formats::RgbaImage ReadPaving(FileView& file)
{
    std::optional<std::size_t> texture = Formats::FindResource(file.All(), TextureGroup, TextureName);
    std::optional<std::size_t> palette = Formats::FindResource(file.All(), PaletteGroup, PaletteName);
    if (!texture || !palette)
        file.Fail("the paving picture or its palette is missing");

    std::uint16_t width = file.Uint16(*texture + TextureWidthPosition);
    std::uint16_t height = file.Uint16(*texture + TextureHeightPosition);
    std::optional<Formats::TextureFormat> format = Formats::TextureFormatFromNumber(file.Uint32(*texture + TextureFormatPosition));
    std::optional<Formats::PaletteFormat> paletteFormat = Formats::PaletteFormatFromNumber(file.Uint32(*palette + PaletteFormatPosition));
    if (!format || !paletteFormat)
        file.Fail("the paving picture has a format that is not known");

    Formats::TexturePalette colors;
    colors.format = *paletteFormat;
    colors.entryCount = file.Uint16(*palette + PaletteCountPosition);
    Formats::ByteView entries = file.Bytes(file.Offset(*palette, *palette + PaletteDataPosition), colors.entryCount * sizeof(std::uint16_t));
    colors.entries.assign(entries.begin(), entries.end());

    std::size_t data = file.Offset(*texture, *texture + TextureDataPosition);
    Formats::ByteView pixels = file.Bytes(data, Formats::EncodedImageSize(*format, width, height));
    return Formats::DecodeTexture(*format, width, height, pixels, colors);
}

}

PlazaGroundModel ReadPlazaGroundModel(Formats::ByteView brres)
{
    FileView file(brres);
    std::optional<std::size_t> model = Formats::FindResource(brres, ModelGroup, ModelName);
    if (!model)
        file.Fail("there is no model graund_model");

    GroundArrays arrays;
    arrays.positions = ReadPositions(file, FirstEntryOfSection(file, *model, PositionSection));
    arrays.colors = ReadColors(file, FirstEntryOfSection(file, *model, ColorSection));
    arrays.textureCoordinates = ReadTextureCoordinates(file, FirstEntryOfSection(file, *model, TextureCoordinateSection));

    PlazaGroundModel ground;
    ground.triangleCorners = ReadTriangles(file, FirstEntryOfSection(file, *model, ObjectSection), arrays);
    ground.paving = ReadPaving(file);
    return ground;
}

PlazaGround::PlazaGround(Graphics::GraphicsDevice& graphicsDevice, const PlazaGroundModel& model)
    : device(graphicsDevice)
    , mesh(graphicsDevice.CreateMesh(model.triangleCorners))
{
    Graphics::TextureDescription description;
    description.width = model.paving.width;
    description.height = model.paving.height;
    description.pixels = model.paving.pixels;
    description.horizontalWrap = Graphics::WrapMode::Mirror;
    description.verticalWrap = Graphics::WrapMode::Mirror;
    paving = device.CreateTexture(description);
}

PlazaGround::~PlazaGround()
{
    device.DestroyMesh(mesh);
    device.DestroyTexture(paving);
}

void PlazaGround::Draw(const Graphics::Matrix4& modelView, const Graphics::Matrix4& projection) const
{
    Graphics::TevStage stage;
    stage.textureIndex = 0;
    stage.usesRasterColor = true;
    stage.color.inputA = Graphics::TevColorInput::TextureColor;
    stage.color.inputB = Graphics::TevColorInput::One;
    stage.color.inputC = Graphics::TevColorInput::RasterAlpha;
    stage.alpha.inputD = Graphics::TevAlphaInput::TextureAlpha;

    Graphics::Material material;
    material.blendMode = Graphics::BlendMode::Replace;
    material.textures = {{paving, {{{{PavingRepeat, 0.0f, PavingShift}, {0.0f, PavingRepeat, 0.0f}}}}}};
    material.stages = {stage};

    Graphics::MeshPlacement placement;
    placement.modelView = modelView;
    placement.projection = projection;
    placement.usesCornerColors = true;
    placement.culling = Graphics::FaceCulling::Back;
    device.DrawMesh(mesh, placement, material);
}

}
