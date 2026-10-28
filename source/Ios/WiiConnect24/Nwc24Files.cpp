#include "Ios/WiiConnect24/Nwc24Files.h"

#include "Formats/ByteReader.h"
#include "Ios/BigEndianBytes.h"

#include <string>
#include <string_view>

namespace Ios
{

namespace
{

constexpr std::size_t WordSize = sizeof(std::uint32_t);

// The version of the lists and of the control files of the mailboxes.
constexpr std::uint32_t FriendListVersion = 2;
constexpr std::uint32_t FriendRecordListVersion = 2;
constexpr std::uint32_t DownloadListVersion = 1;
constexpr std::uint32_t MailboxControlVersion = 4;

// The message config

constexpr std::size_t MessageConfigVersionOffset = 4;
constexpr std::size_t MailDomainOffset = 0x18;
constexpr std::size_t MailDomainSize = 0x40;
constexpr std::size_t PasswordOffset = 0x58;
constexpr std::size_t PasswordSize = 0x20;
constexpr std::size_t MailCheckIdOffset = 0x78;
constexpr std::size_t MailCheckIdSize = 0x24;
constexpr std::size_t UrlSize = 0x80;
constexpr std::size_t ChecksumOffset = MessageConfigSize - WordSize;

// The addresses of the servers and the values of a console that has no account. The product never connects to them.
constexpr std::string_view MailDomain = "@wii.com";
constexpr std::string_view EmptyPassword = "0000000000000000";
constexpr std::string_view EmptyMailCheckId = "9999999999999999";

struct ServerUrl
{
    std::size_t offset;
    std::string_view text;
};

constexpr ServerUrl ServerUrls[] =
{
    {0x9C, "https://amw.wc24.wii.com/cgi-bin/account.cgi"},
    {0x11C, "http://rcw.wc24.wii.com/cgi-bin/check.cgi"},
    {0x19C, "https://mtw.wc24.wii.com/cgi-bin/receive.cgi"},
    {0x21C, "https://mtw.wc24.wii.com/cgi-bin/delete.cgi"},
    {0x29C, "https://mtw.wc24.wii.com/cgi-bin/send.cgi"},
};

// The checksum is the sum of all the words of the file in front of it.
std::uint32_t ChecksumOfMessageConfig(Formats::ByteView bytes)
{
    Formats::ByteReader reader(bytes, "Message config");
    std::uint32_t sum = 0;
    for (std::size_t offset = 0; offset < ChecksumOffset; offset += WordSize)
        sum += reader.ReadUint32();

    return sum;
}

// The lists. Their headers start with the magic word and the version.

constexpr std::size_t ListVersionOffset = 4;
constexpr std::size_t ListEntryCountOffset = 8;

void PutListHeader(Formats::Bytes& bytes, std::uint32_t magic, std::uint32_t version, std::uint32_t entryCount)
{
    PutUint32(bytes, 0, magic);
    PutUint32(bytes, ListVersionOffset, version);
    PutUint32(bytes, ListEntryCountOffset, entryCount);
}

// The header of the download list is different. Two of its numbers are always the same in a new file, and what they mean is not known.
constexpr std::size_t DownloadListUnknownNumberAt10Offset = 0x10;
constexpr std::size_t DownloadListUnknownNumberAt12Offset = 0x12;
constexpr std::size_t DownloadListEntryCountOffset = 0x14;
constexpr std::uint16_t DownloadListUnknownNumberAt10 = 0x20;
constexpr std::uint16_t DownloadListUnknownNumberAt12 = 8;

// Every entry of the download list starts with its own number and has 0xFF as its third byte.
constexpr std::size_t DownloadEntryThirdByteOffset = 2;
constexpr Formats::Byte DownloadEntryThirdByte = 0xFF;

// The control files of the mailboxes

constexpr std::size_t NextEntryOffsetInEntry = 0xC;

// The places of the numbers in the header of a control file.
constexpr std::size_t ControlVersionOffset = 0x04;
constexpr std::size_t ControlEntryCountOffset = 0x0C;
constexpr std::size_t ControlFileSizeOffset = 0x14;
constexpr std::size_t ControlFirstChangingWordOffset = 0x18;
constexpr std::size_t ControlSecondChangingWordOffset = 0x1C;
constexpr std::size_t ControlFreeBytesOffset = 0x24;

// Three words of the header change while the mailbox is in use. A new file has the values that the SDK writes: 1, 0x80, and the
// whole mailbox as free space. The receive mailbox of the reference console, which was used, holds 5, 0x200 and 0x6F0A00.
// What the first two words count is not known.
constexpr std::uint32_t ControlFirstChangingWord = 1;
constexpr std::uint32_t ControlSecondChangingWord = 0x80;

// The header ends with a row of zero characters, the place of a message number that does not exist yet.
constexpr std::size_t EmptyNumberOffset = 0x58;
constexpr std::size_t EmptyNumberLength = 39;

}

Formats::Bytes MakeMessageConfig()
{
    Formats::Bytes bytes(MessageConfigSize);
    PutUint32(bytes, 0, MessageConfigMagic);
    PutUint32(bytes, MessageConfigVersionOffset, MessageConfigVersion);
    PutText(bytes, MailDomainOffset, MailDomain, MailDomainSize);
    PutText(bytes, PasswordOffset, EmptyPassword, PasswordSize);
    PutText(bytes, MailCheckIdOffset, EmptyMailCheckId, MailCheckIdSize);

    for (const ServerUrl& url : ServerUrls)
        PutText(bytes, url.offset, url.text, UrlSize);

    PutUint32(bytes, ChecksumOffset, ChecksumOfMessageConfig(bytes));
    return bytes;
}

bool IsMessageConfig(Formats::ByteView bytes)
{
    if (bytes.size() != MessageConfigSize)
        return false;

    Formats::ByteReader reader(bytes, "Message config");
    if (reader.ReadUint32() != MessageConfigMagic || reader.ReadUint32() != MessageConfigVersion)
        return false;

    reader.MoveTo(ChecksumOffset);
    return reader.ReadUint32() == ChecksumOfMessageConfig(bytes);
}

Formats::Bytes MakeFriendList()
{
    Formats::Bytes bytes(FriendListSize);
    PutListHeader(bytes, FriendListMagic, FriendListVersion, FriendListEntryCount);
    return bytes;
}

Formats::Bytes MakeFriendRecordList()
{
    Formats::Bytes bytes(FriendRecordListSize);
    PutListHeader(bytes, FriendRecordListMagic, FriendRecordListVersion, FriendRecordListEntryCount);
    return bytes;
}

Formats::Bytes MakeDownloadList()
{
    Formats::Bytes bytes(DownloadListSize);
    PutListHeader(bytes, DownloadListMagic, DownloadListVersion, 0);
    PutUint16(bytes, DownloadListUnknownNumberAt10Offset, DownloadListUnknownNumberAt10);
    PutUint16(bytes, DownloadListUnknownNumberAt12Offset, DownloadListUnknownNumberAt12);
    PutUint16(bytes, DownloadListEntryCountOffset, static_cast<std::uint16_t>(DownloadListEntryCount));

    for (std::size_t entryNumber = 0; entryNumber < DownloadListEntryCount; ++entryNumber)
    {
        std::size_t entryStart = DownloadListHeaderSize + entryNumber * DownloadListEntrySize;
        PutUint16(bytes, entryStart, static_cast<std::uint16_t>(entryNumber));
        PutByte(bytes, entryStart + DownloadEntryThirdByteOffset, DownloadEntryThirdByte);
    }
    return bytes;
}

bool IsList(Formats::ByteView bytes, std::uint32_t magic, std::size_t size)
{
    if (bytes.size() != size)
        return false;

    Formats::ByteReader reader(bytes, "List");
    return reader.ReadUint32() == magic;
}

Formats::Bytes MakeMailboxControl(Mailbox mailbox)
{
    Formats::Bytes bytes(MailboxControlSize(mailbox));
    std::uint32_t fileSize = static_cast<std::uint32_t>(bytes.size());

    PutUint32(bytes, 0, MailboxControlMagic);
    PutUint32(bytes, ControlVersionOffset, MailboxControlVersion);
    PutUint32(bytes, ControlEntryCountOffset, MailboxEntryCount(mailbox));
    PutUint32(bytes, ControlFileSizeOffset, fileSize);
    PutUint32(bytes, ControlFirstChangingWordOffset, ControlFirstChangingWord);
    PutUint32(bytes, ControlSecondChangingWordOffset, ControlSecondChangingWord);
    PutUint32(bytes, ControlFreeBytesOffset, MailboxVolumeSize(mailbox));
    PutText(bytes, EmptyNumberOffset, std::string(EmptyNumberLength, '0'), EmptyNumberLength);

    for (std::size_t entryStart = MailboxControlHeaderSize; entryStart < bytes.size(); entryStart += MailboxControlEntrySize)
    {
        std::size_t nextEntry = (entryStart + MailboxControlEntrySize) % bytes.size();
        PutUint32(bytes, entryStart + NextEntryOffsetInEntry, static_cast<std::uint32_t>(nextEntry));
    }
    return bytes;
}

bool IsMailboxControl(Formats::ByteView bytes, Mailbox mailbox)
{
    if (bytes.size() != MailboxControlSize(mailbox))
        return false;

    Formats::ByteReader reader(bytes, "Mailbox control");
    return reader.ReadUint32() == MailboxControlMagic && reader.ReadUint32() == MailboxControlVersion;
}

}
