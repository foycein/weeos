#include "Boot/LaunchFile.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"

namespace Boot
{

namespace
{

Ios::NandPath LaunchFilePath()
{
    return Ios::NandPath::Parse("/sys/launch.sys").Value();
}

Ios::NandPath SpaceFilePath()
{
    return Ios::NandPath::Parse("/sys/space.sys").Value();
}

Formats::Bytes WriteRequest(const LaunchRequest& request)
{
    Formats::ByteWriter writer;
    writer.WriteUint64(request.titleId.ToValue());
    writer.WriteBytes(Ios::WriteTicketView(request.ticketView));
    return writer.GetBytes();
}

Ios::Result<LaunchRequest> ParseRequest(Formats::ByteView bytes)
{
    if (bytes.size() != LaunchFileSize)
        return Ios::IosError::ShortRead;

    Formats::ByteReader reader(bytes, "Launch file");
    Ios::TitleId titleId = Ios::TitleId::FromValue(reader.ReadUint64());

    Ios::Result<Ios::TicketView> ticketView = Ios::ParseTicketView(reader.ReadBytes(Ios::TicketViewSize), Ios::IosError::ShortRead);
    if (ticketView.Failed())
        return ticketView.Error();

    return LaunchRequest{titleId, ticketView.Value()};
}

}

LaunchFile::LaunchFile(Ios::FileSystemService& fileSystem)
    : files(fileSystem)
{
}

Ios::Result<void> LaunchFile::Write(const LaunchRequest& request)
{
    Ios::Result<void> spaceGivenUp = files.DeleteIfPresent(SpaceFilePath());
    if (spaceGivenUp.Failed())
        return spaceGivenUp;

    return files.WriteFile(LaunchFilePath(), WriteRequest(request));
}

Ios::Result<LaunchRequest> LaunchFile::Read() const
{
    Ios::Result<Formats::Bytes> bytes = files.ReadFile(LaunchFilePath());
    if (bytes.Failed())
        return bytes.Error();

    return ParseRequest(bytes.Value());
}

Ios::Result<LaunchRequest> LaunchFile::Take()
{
    Ios::Result<LaunchRequest> request = Read();
    if (request.Failed())
        return request;

    Ios::Result<void> deleted = files.Delete(LaunchFilePath());
    if (deleted.Failed())
        return deleted.Error();

    Ios::Result<void> spaceBack = files.WriteFile(SpaceFilePath(), Formats::Bytes(SpaceFileSize));
    if (spaceBack.Failed())
        return spaceBack.Error();

    return request;
}

}
