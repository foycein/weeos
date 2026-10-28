#include "Formats/U8Archive.h"

#include "Formats/ByteReader.h"

#include <format>

namespace Formats
{

namespace
{

constexpr std::uint32_t ArchiveTag = 0x55AA382D;
constexpr std::uint32_t ExpectedFirstNodeOffset = 0x20;
constexpr std::size_t HeaderUnusedSize = 16;
constexpr std::size_t NodeSize = 12;
constexpr Byte FileNodeType = 0;
constexpr Byte DirectoryNodeType = 1;
constexpr int NameOffsetBitCount = 24;

struct ArchiveHeader
{
    std::size_t firstNodeOffset;
    std::size_t nodesAndNamesSize;
    std::size_t dataOffset;
};

// A file node holds the offset and the size of the data. A directory node holds the index of its parent
// and the index of the first node after its last entry.
struct Node
{
    Byte type;
    std::uint32_t nameOffset;
    std::uint32_t dataOffsetOrParentIndex;
    std::uint32_t sizeOrEndIndex;
};

// A directory whose entries are still being read.
struct OpenDirectory
{
    std::string name;
    std::uint32_t endIndex;
};

ArchiveHeader ReadArchiveHeader(ByteReader& reader)
{
    std::uint32_t tag = reader.ReadUint32();
    if (tag != ArchiveTag)
        reader.Fail(std::format("the tag {:#x} is not the U8 tag {:#x}", tag, ArchiveTag));

    ArchiveHeader header;
    header.firstNodeOffset = reader.ReadUint32();
    if (header.firstNodeOffset != ExpectedFirstNodeOffset)
        reader.Fail(std::format("the first node is at {:#x} instead of {:#x}", header.firstNodeOffset, ExpectedFirstNodeOffset));

    header.nodesAndNamesSize = reader.ReadUint32();
    header.dataOffset = reader.ReadUint32();
    reader.Skip(HeaderUnusedSize);
    return header;
}

Node ReadNode(ByteReader& reader)
{
    std::uint32_t typeAndNameOffset = reader.ReadUint32();

    Node node;
    node.type = static_cast<Byte>(typeAndNameOffset >> NameOffsetBitCount);
    node.nameOffset = typeAndNameOffset & ((1u << NameOffsetBitCount) - 1);
    node.dataOffsetOrParentIndex = reader.ReadUint32();
    node.sizeOrEndIndex = reader.ReadUint32();
    return node;
}

// The first node is the root directory. It has no name, and its size field is the number of nodes.
std::uint32_t ReadNodeCount(ByteReader& reader, const ArchiveHeader& header)
{
    Node rootNode = ReadNode(reader);
    if (rootNode.type != DirectoryNodeType)
        reader.Fail("the first node is not a directory");

    std::uint32_t nodeCount = rootNode.sizeOrEndIndex;
    std::uint64_t nodesSize = std::uint64_t(nodeCount) * NodeSize;
    if (nodeCount == 0 || nodesSize > header.nodesAndNamesSize)
        reader.Fail(std::format("the {} nodes do not fit into the header of {:#x} bytes", nodeCount, header.nodesAndNamesSize));

    return nodeCount;
}

ByteView FindNameTable(ByteView archiveBytes, const ArchiveHeader& header, std::uint32_t nodeCount)
{
    std::size_t nodesSize = std::size_t(nodeCount) * NodeSize;

    ByteReader reader(archiveBytes, "U8 archive");
    reader.MoveTo(header.firstNodeOffset + nodesSize);
    return reader.ReadBytes(header.nodesAndNamesSize - nodesSize);
}

std::string ReadName(ByteView nameTable, std::uint32_t nameOffset)
{
    ByteReader reader(nameTable, "U8 archive name table");
    reader.MoveTo(nameOffset);
    return reader.ReadTextUntilZero();
}

std::string JoinPath(const std::vector<OpenDirectory>& openDirectories, const std::string& name)
{
    std::string path;
    for (const OpenDirectory& directory : openDirectories)
        path += directory.name + "/";

    return path + name;
}

void CloseFinishedDirectories(std::vector<OpenDirectory>& openDirectories, std::uint32_t nodeIndex)
{
    while (!openDirectories.empty() && nodeIndex >= openDirectories.back().endIndex)
        openDirectories.pop_back();
}

U8Entry MakeFileEntry(const ByteReader& reader, const ArchiveHeader& header, const std::string& path, const Node& node,
    std::size_t archiveSize)
{
    if (node.dataOffsetOrParentIndex < header.dataOffset || !IsRangeInside(node.dataOffsetOrParentIndex, node.sizeOrEndIndex, archiveSize))
        reader.Fail(std::format("the data of the file {} lies outside the archive", path));

    U8Entry entry;
    entry.path = path;
    entry.isDirectory = false;
    entry.dataOffset = node.dataOffsetOrParentIndex;
    entry.dataSize = node.sizeOrEndIndex;
    return entry;
}

U8Entry MakeDirectoryEntry(const std::string& path)
{
    U8Entry entry;
    entry.path = path;
    entry.isDirectory = true;
    entry.dataOffset = 0;
    entry.dataSize = 0;
    return entry;
}

void CheckDirectoryEnd(const ByteReader& reader, const std::vector<OpenDirectory>& openDirectories, const Node& node,
    std::uint32_t nodeIndex, std::uint32_t nodeCount, const std::string& path)
{
    bool endsAfterItself = node.sizeOrEndIndex > nodeIndex;
    bool endsInsideTheArchive = node.sizeOrEndIndex <= nodeCount;
    bool endsInsideItsParent = openDirectories.empty() || node.sizeOrEndIndex <= openDirectories.back().endIndex;

    if (!endsAfterItself || !endsInsideTheArchive || !endsInsideItsParent)
        reader.Fail(std::format("the directory {} ends at the impossible node {}", path, node.sizeOrEndIndex));
}

std::vector<U8Entry> ReadEntries(ByteReader& reader, const ArchiveHeader& header, ByteView nameTable, std::uint32_t nodeCount)
{
    std::vector<U8Entry> entries;
    std::vector<OpenDirectory> openDirectories;

    for (std::uint32_t nodeIndex = 1; nodeIndex < nodeCount; ++nodeIndex)
    {
        Node node = ReadNode(reader);
        CloseFinishedDirectories(openDirectories, nodeIndex);

        std::string name = ReadName(nameTable, node.nameOffset);
        std::string path = JoinPath(openDirectories, name);

        if (node.type == FileNodeType)
        {
            entries.push_back(MakeFileEntry(reader, header, path, node, reader.Size()));
        }
        else if (node.type == DirectoryNodeType)
        {
            CheckDirectoryEnd(reader, openDirectories, node, nodeIndex, nodeCount, path);
            openDirectories.push_back({name, node.sizeOrEndIndex});
            entries.push_back(MakeDirectoryEntry(path));
        }
        else
        {
            reader.Fail(std::format("the node {} has the unknown type {}", path, node.type));
        }
    }

    return entries;
}

}

std::optional<ByteView> U8Archive::FindFile(std::string_view path) const&
{
    if (path.starts_with('/'))
        path.remove_prefix(1);

    for (const U8Entry& entry : entries)
    {
        if (!entry.isDirectory && entry.path == path)
            return FileData(entry);
    }

    return std::nullopt;
}

ByteView U8Archive::FileData(const U8Entry& entry) const&
{
    return ByteView(bytes).subspan(entry.dataOffset, entry.dataSize);
}

bool IsU8Archive(ByteView bytes)
{
    if (bytes.size() < sizeof(ArchiveTag))
        return false;

    ByteReader reader(bytes, "U8 archive");
    return reader.ReadUint32() == ArchiveTag;
}

U8Archive ReadU8Archive(ByteView bytes)
{
    ByteReader reader(bytes, "U8 archive");
    ArchiveHeader header = ReadArchiveHeader(reader);

    reader.MoveTo(header.firstNodeOffset);
    std::uint32_t nodeCount = ReadNodeCount(reader, header);
    ByteView nameTable = FindNameTable(bytes, header, nodeCount);

    U8Archive archive;
    archive.bytes.assign(bytes.begin(), bytes.end());
    archive.entries = ReadEntries(reader, header, nameTable, nodeCount);
    return archive;
}

}
