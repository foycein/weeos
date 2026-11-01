#include "Media/MovieFrames.h"

#include "Media/JpegDecoder.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <optional>
#include <string_view>

namespace Media
{

namespace
{

constexpr std::string_view DataDescription = "Motion-JPEG frame";

constexpr Formats::Byte MarkerPrefix = 0xFF;
constexpr Formats::Byte StartOfImage = 0xD8;
constexpr Formats::Byte EndOfImage = 0xD9;
constexpr Formats::Byte AppSegment1 = 0xE1;
constexpr Formats::Byte StuffedZero = 0x00;
constexpr Formats::Byte DefineQuantizationTables = 0xDB;
constexpr Formats::Byte DefineHuffmanTables = 0xC4;
constexpr Formats::Byte BaselineFrame = 0xC0;
constexpr Formats::Byte StartOfScan = 0xDA;
constexpr int TwoFields = 2;

// The first segment of a JPEG file follows its SOI marker; its content follows the marker and the length.
constexpr std::size_t FirstSegmentPlace = 2;
constexpr std::size_t FirstSegmentContentPlace = 6;

// The header of a Motion-JPEG field (Apple's QuickTime File Format): 4 reserved bytes, the tag "mjpg", the size of the field,
// its padded size, the place of the next field, then the places of the quantization tables, the Huffman tables, the frame
// header, the scan header and the data. Every place counts from the start of the field. Motion-JPEG A keeps this header in an
// APP1 segment, Motion-JPEG B puts it at the start of the field instead of the markers.
constexpr std::size_t FieldHeaderSize = 40;
constexpr std::size_t FieldTagPlace = 4;
constexpr std::string_view FieldTag = "mjpg";

struct FieldHeader
{
    std::uint32_t fieldSize = 0;
    std::uint32_t paddedFieldSize = 0;
    std::uint32_t nextFieldPlace = 0;
    std::uint32_t quantizationTablesPlace = 0;
    std::uint32_t huffmanTablesPlace = 0;
    std::uint32_t frameHeaderPlace = 0;
    std::uint32_t scanHeaderPlace = 0;
    std::uint32_t dataPlace = 0;
};

std::optional<FieldHeader> ReadFieldHeader(Formats::ByteView headerBytes)
{
    if (headerBytes.size() < FieldHeaderSize)
        return std::nullopt;

    std::string_view tag(reinterpret_cast<const char*>(headerBytes.data()) + FieldTagPlace, FieldTag.size());
    if (tag != FieldTag)
        return std::nullopt;

    Formats::ByteReader reader(headerBytes, DataDescription);
    reader.MoveTo(FieldTagPlace + FieldTag.size());
    FieldHeader header;
    header.fieldSize = reader.ReadUint32();
    header.paddedFieldSize = reader.ReadUint32();
    header.nextFieldPlace = reader.ReadUint32();
    header.quantizationTablesPlace = reader.ReadUint32();
    header.huffmanTablesPlace = reader.ReadUint32();
    header.frameHeaderPlace = reader.ReadUint32();
    header.scanHeaderPlace = reader.ReadUint32();
    header.dataPlace = reader.ReadUint32();
    return header;
}

// The field header in the APP1 segment of a Motion-JPEG A field, when the segment comes right after SOI.
std::optional<FieldHeader> ReadMotionJpegAFieldHeader(Formats::ByteView field)
{
    if (field.size() < FirstSegmentContentPlace + FieldHeaderSize)
        return std::nullopt;

    if (field[FirstSegmentPlace] != MarkerPrefix || field[FirstSegmentPlace + 1] != AppSegment1)
        return std::nullopt;

    return ReadFieldHeader(field.subspan(FirstSegmentContentPlace));
}

// Where the second field starts: the place of the field header, or else the next SOI marker.
std::size_t FindSecondField(Formats::ByteView frame)
{
    std::optional<FieldHeader> header = ReadMotionJpegAFieldHeader(frame);
    if (header && header->nextFieldPlace > 0 && header->nextFieldPlace < frame.size())
        return header->nextFieldPlace;

    for (std::size_t place = FirstSegmentPlace; place + 1 < frame.size(); ++place)
    {
        if (frame[place] == MarkerPrefix && frame[place + 1] == StartOfImage)
            return place;
    }
    throw Formats::FormatError("Motion-JPEG frame: the frame has no second field");
}

// The even lines come from the first field, the odd lines from the second.
Formats::RgbaImage WeaveFields(const Formats::RgbaImage& firstField, const Formats::RgbaImage& secondField)
{
    Formats::RgbaImage frame;
    frame.width = firstField.width;
    frame.height = firstField.height + secondField.height;
    frame.pixels.reserve(static_cast<std::size_t>(frame.width) * frame.height);

    for (std::uint32_t line = 0; line < frame.height; ++line)
    {
        const Formats::RgbaImage& field = line % 2 == 0 ? firstField : secondField;
        std::uint32_t fieldLine = std::min(line / 2, field.height - 1);
        auto lineStart = field.pixels.begin() + static_cast<std::ptrdiff_t>(fieldLine) * field.width;
        std::uint32_t copiedWidth = std::min(frame.width, field.width);
        frame.pixels.insert(frame.pixels.end(), lineStart, lineStart + copiedWidth);
        frame.pixels.resize(frame.pixels.size() + (frame.width - copiedWidth));
    }
    return frame;
}

void AppendSegment(Formats::ByteView field, std::uint32_t place, Formats::Byte marker, Formats::Bytes& jpeg)
{
    if (place == 0)
        return;

    Formats::ByteReader reader(field, DataDescription);
    reader.MoveTo(place);
    std::uint16_t segmentLength = reader.ReadUint16();
    reader.MoveTo(place);
    Formats::ByteView segment = reader.ReadBytes(segmentLength);

    jpeg.push_back(MarkerPrefix);
    jpeg.push_back(marker);
    jpeg.insert(jpeg.end(), segment.begin(), segment.end());
}

Formats::RgbaImage DecodeField(MovieVideoKind kind, Formats::ByteView field)
{
    if (kind == MovieVideoKind::MotionJpegB)
        return DecodeJpeg(MotionJpegBFieldToJpeg(field));

    return DecodeJpeg(field);
}

std::size_t SecondFieldStart(MovieVideoKind kind, Formats::ByteView frame)
{
    if (kind != MovieVideoKind::MotionJpegB)
        return FindSecondField(frame);

    std::optional<FieldHeader> header = ReadFieldHeader(frame);
    if (!header || header->nextFieldPlace == 0 || header->nextFieldPlace >= frame.size())
        throw Formats::FormatError("Motion-JPEG B frame: the frame has no second field");

    return header->nextFieldPlace;
}

}

Formats::Bytes MotionJpegBFieldToJpeg(Formats::ByteView field)
{
    std::optional<FieldHeader> header = ReadFieldHeader(field);
    if (!header)
        throw Formats::FormatError("Motion-JPEG B frame: the field does not start with the tag mjpg");

    Formats::Bytes jpeg = {MarkerPrefix, StartOfImage};
    AppendSegment(field, header->quantizationTablesPlace, DefineQuantizationTables, jpeg);
    AppendSegment(field, header->huffmanTablesPlace, DefineHuffmanTables, jpeg);
    AppendSegment(field, header->frameHeaderPlace, BaselineFrame, jpeg);
    AppendSegment(field, header->scanHeaderPlace, StartOfScan, jpeg);

    std::size_t dataEnd = std::min<std::size_t>(header->fieldSize, field.size());
    for (std::size_t place = header->dataPlace; place < dataEnd; ++place)
    {
        jpeg.push_back(field[place]);
        if (field[place] == MarkerPrefix)
            jpeg.push_back(StuffedZero);
    }

    jpeg.push_back(MarkerPrefix);
    jpeg.push_back(EndOfImage);
    return jpeg;
}

Formats::RgbaImage DecodeMovieFrame(const MovieVideoTrack& video, Formats::ByteView frameBytes)
{
    if (video.kind == MovieVideoKind::Unsupported)
        throw Formats::FormatError("Motion-JPEG frame: the codec " + video.codecName + " is not supported");

    if (video.fieldCount != TwoFields)
        return DecodeField(video.kind, frameBytes);

    std::size_t secondFieldStart = SecondFieldStart(video.kind, frameBytes);
    Formats::RgbaImage firstField = DecodeField(video.kind, frameBytes.first(secondFieldStart));
    Formats::RgbaImage secondField = DecodeField(video.kind, frameBytes.subspan(secondFieldStart));
    return WeaveFields(firstField, secondField);
}

}
