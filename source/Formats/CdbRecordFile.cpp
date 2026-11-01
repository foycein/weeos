#include "Formats/CdbRecordFile.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/FormatError.h"

#include <algorithm>
#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view Magic = "CDBFILE";
constexpr Byte NewestVersion = 2;
constexpr Byte FirstVersion = 1;
constexpr std::string_view Description = "CDB record";

// The length of the type name, with its zero, is one byte: at 0x10 in version 2, at 0x0D in version 1.
constexpr std::size_t NameLengthOffset = 0x10;
constexpr std::size_t FirstVersionNameLengthOffset = 0x0D;
constexpr std::size_t TypeNameOffset = 0x14;
constexpr std::size_t LongestTypeName = 0x58;
constexpr std::size_t NumbersOffset = 0x70;

std::size_t NameLengthOffsetOf(Byte version)
{
    return version == FirstVersion ? FirstVersionNameLengthOffset : NameLengthOffset;
}

}

CdbRecordFile ReadCdbRecordFile(ByteView bytes)
{
    ByteReader reader(bytes, Description);
    if (reader.ReadText(Magic.size()) != Magic)
        reader.Fail("the file does not start with CDBFILE");

    CdbRecordFile record;
    record.version = reader.ReadByte();
    if (record.version > NewestVersion)
        reader.Fail(std::format("version {} is newer than 2", record.version));
    record.consoleId = reader.ReadByteArray<CdbConsoleIdSize>();

    reader.MoveTo(NameLengthOffsetOf(record.version));
    std::size_t nameLength = reader.ReadByte();
    if (nameLength > LongestTypeName)
        reader.Fail(std::format("a type name of {} bytes", nameLength));

    reader.MoveTo(TypeNameOffset);
    ByteView nameBytes = reader.ReadBytes(nameLength);
    auto zero = std::find(nameBytes.begin(), nameBytes.end(), Byte{0});
    if (zero == nameBytes.end())
        reader.Fail("the type name has no end");
    record.typeName.assign(nameBytes.begin(), zero);

    reader.MoveTo(NumbersOffset);
    record.recordNumber = reader.ReadUint32();
    record.writeCount = reader.ReadUint32();
    record.sizeOnSdCard = reader.ReadUint32();
    record.modifiedTime = reader.ReadUint32();
    record.restOfHeader = reader.ReadByteArray<CdbRecordHeaderSize - 0x80>();

    ByteView data = reader.ReadBytes(reader.RemainingSize());
    record.data.assign(data.begin(), data.end());
    return record;
}

Bytes WriteCdbRecordFile(const CdbRecordFile& record)
{
    if (record.typeName.size() + 1 > LongestTypeName)
        throw FormatError(std::format("CDB record: the type name {} is too long", record.typeName));

    Bytes header(NumbersOffset, 0);
    std::copy(Magic.begin(), Magic.end(), header.begin());
    header[Magic.size()] = record.version;
    std::copy(record.consoleId.begin(), record.consoleId.end(), header.begin() + Magic.size() + 1);
    header[NameLengthOffsetOf(record.version)] = static_cast<Byte>(record.typeName.size() + 1);
    std::copy(record.typeName.begin(), record.typeName.end(), header.begin() + TypeNameOffset);

    ByteWriter file;
    file.WriteBytes(header);
    file.WriteUint32(record.recordNumber);
    file.WriteUint32(record.writeCount);
    file.WriteUint32(record.sizeOnSdCard);
    file.WriteUint32(record.modifiedTime);
    file.WriteBytes(record.restOfHeader);
    file.WriteBytes(record.data);
    return file.GetBytes();
}

}
