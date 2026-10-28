#include "Ios/Title/SharedContentMap.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Ios/Title/TitlePaths.h"

#include <algorithm>

namespace Ios
{

namespace
{

constexpr std::size_t NameSize = 8;
constexpr std::size_t EntrySize = NameSize + Formats::Sha1DigestSize;

SharedContentEntry ReadEntry(Formats::ByteReader& reader)
{
    SharedContentEntry entry;
    entry.name = reader.ReadText(NameSize);
    entry.hash = reader.ReadByteArray<Formats::Sha1DigestSize>();
    return entry;
}

}

Result<SharedContentMap> SharedContentMap::Read(const KernelFiles& files)
{
    Result<Formats::Bytes> bytes = files.ReadFile(SharedContentMapPath());
    if (bytes.Failed() && bytes.Error() == IosError::NotFound)
        return SharedContentMap();

    if (bytes.Failed())
        return bytes.Error();

    bool endsInTheMiddleOfAnEntry = bytes.Value().size() % EntrySize != 0;
    if (endsInTheMiddleOfAnEntry)
        return IosError::ShortRead;

    SharedContentMap map;
    Formats::ByteReader reader(bytes.Value(), "Shared content map");
    while (!reader.IsAtEnd())
        map.entries.push_back(ReadEntry(reader));

    return map;
}

const std::vector<SharedContentEntry>& SharedContentMap::Entries() const
{
    return entries;
}

std::optional<std::string> SharedContentMap::FindName(const Formats::Sha1Digest& hash) const
{
    auto found = std::find_if(entries.begin(), entries.end(), [&hash](const SharedContentEntry& entry) { return entry.hash == hash; });
    if (found == entries.end())
        return std::nullopt;

    return found->name;
}

std::string SharedContentMap::MakeNextName() const
{
    return FormatNameNumber(static_cast<std::uint32_t>(entries.size()));
}

void SharedContentMap::Add(const SharedContentEntry& entry)
{
    entries.push_back(entry);
}

Result<void> SharedContentMap::Write(KernelFiles& files) const
{
    Formats::ByteWriter writer;
    for (const SharedContentEntry& entry : entries)
    {
        writer.WriteBytes(Formats::ByteView(reinterpret_cast<const Formats::Byte*>(entry.name.data()), entry.name.size()));
        writer.WriteBytes(entry.hash);
    }

    return files.WriteFile(SharedContentMapPath(), writer.GetBytes());
}

}
