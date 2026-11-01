#include "SystemMenu/BackgroundServices/PlayTimeLog.h"

#include "SystemMenu/MenuError.h"

#include "Formats/FormatError.h"

#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view PlayRecordPath = "/title/00000001/00000002/data/play_rec.dat";

std::optional<Formats::PlayRecord> ReadGoodRecord(NandFileAccess& files, const Ios::NandPath& path)
{
    Ios::Result<Formats::Bytes> bytes = files.Read(path);
    if (bytes.Failed())
        return std::nullopt;

    try
    {
        return Formats::ReadPlayRecord(bytes.Value());
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
}

}

void WritePlayRecord(NandFileAccess& files, const Formats::PlayRecord& record)
{
    Ios::NandPath path = MenuPath(PlayRecordPath);
    files.Create(path, EverybodyReadsAndWrites);
    files.Write(path, Formats::WritePlayRecord(record));
}

std::optional<Formats::PlayRecord> TakePlayRecord(NandFileAccess& files)
{
    Ios::NandPath path = MenuPath(PlayRecordPath);
    std::optional<Formats::PlayRecord> record = ReadGoodRecord(files, path);
    if (!record)
        return std::nullopt;

    Ios::Result<void> deleted = files.Delete(path);
    if (deleted.Failed())
        throw MenuError(std::format("The play record cannot be deleted (result code {})", Ios::OriginalResultCode(deleted.Error())));

    return record;
}

}
