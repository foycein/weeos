#pragma once

#include "Formats/Bytes.h"

#include <cstddef>
#include <cstdint>

namespace Ios
{

// The files that the WiiConnect24 programs keep in /shared2/wc24: the account of the console, the lists of friends and
// downloads, and the two mailboxes of the Message Board. These functions make them as the SDK's NWC24 library makes them
// when it starts for the first time (docs/audit/Ios.md). All numbers in the files are big endian.

// The mailbox of mail that the console sends, and the one of mail that it received.
enum class Mailbox
{
    Send,
    Receive,
};

// The files wc24send.mbx and wc24recv.mbx hold the messages. Each one is a small FAT file system inside a file.
constexpr std::uint32_t SendMailboxVolumeSize = 0x200000;
constexpr std::uint32_t ReceiveMailboxVolumeSize = 0x700000;
constexpr std::uint32_t MailboxVolumeSize(Mailbox mailbox)
{
    return mailbox == Mailbox::Send ? SendMailboxVolumeSize : ReceiveMailboxVolumeSize;
}

// The magic words that start the files. They are text: "WcCf" is 0x57634366.
constexpr std::uint32_t MessageConfigMagic = 0x57634366;
constexpr std::uint32_t FriendListMagic = 0x5763466C;
constexpr std::uint32_t FriendRecordListMagic = 0x57634673;
constexpr std::uint32_t DownloadListMagic = 0x5763446C;
constexpr std::uint32_t MailboxControlMagic = 0x57635466;

// nwc24msg.cfg and its backup nwc24msg.cbk: who the console is for the mail service. A new console has not got an account:
// the identity is zero and the creation stage is "initial".
constexpr std::size_t MessageConfigSize = 0x400;
constexpr std::uint32_t MessageConfigVersion = 8;
Formats::Bytes MakeMessageConfig();

// True when the bytes have the size, the magic word and the version of the file, and the checksum is right.
bool IsMessageConfig(Formats::ByteView bytes);

// nwc24fl.bin: the list of friends. A header and 100 entries, all empty.
constexpr std::size_t FriendListHeaderSize = 0x360;
constexpr std::size_t FriendListEntrySize = 0x140;
constexpr std::size_t FriendListEntryCount = 100;
constexpr std::size_t FriendListSize = FriendListHeaderSize + FriendListEntryCount * FriendListEntrySize;
Formats::Bytes MakeFriendList();

// nwc24fls.bin: the second list of friends. A header and 336 entries, all empty.
constexpr std::size_t FriendRecordListHeaderSize = 0x800;
constexpr std::size_t FriendRecordListEntrySize = 0x20;
constexpr std::size_t FriendRecordListEntryCount = 0x150;
constexpr std::size_t FriendRecordListSize = FriendRecordListHeaderSize + FriendRecordListEntryCount * FriendRecordListEntrySize;
Formats::Bytes MakeFriendRecordList();

// nwc24dl.bin: the download tasks. A header and 120 entries, all empty.
constexpr std::size_t DownloadListHeaderSize = 0x800;
constexpr std::size_t DownloadListEntrySize = 0x200;
constexpr std::size_t DownloadListEntryCount = 0x78;
constexpr std::size_t DownloadListSize = DownloadListHeaderSize + DownloadListEntryCount * DownloadListEntrySize;
Formats::Bytes MakeDownloadList();

// True when the bytes have the size and the magic word of the list.
bool IsList(Formats::ByteView bytes, std::uint32_t magic, std::size_t size);

// wc24send.ctl and wc24recv.ctl: the table of the messages in a mailbox. A header and one entry for each message
// the mailbox can hold. In a new file the entries are chained: each one holds the place of the next one in the file, the last one holds 0.
constexpr std::size_t MailboxControlHeaderSize = 0x80;
constexpr std::size_t MailboxControlEntrySize = 0x80;
constexpr std::uint32_t SendMailboxEntryCount = 0x7F;
constexpr std::uint32_t ReceiveMailboxEntryCount = 0xFF;
constexpr std::uint32_t MailboxEntryCount(Mailbox mailbox)
{
    return mailbox == Mailbox::Send ? SendMailboxEntryCount : ReceiveMailboxEntryCount;
}
constexpr std::size_t MailboxControlSize(Mailbox mailbox)
{
    return MailboxControlHeaderSize + MailboxEntryCount(mailbox) * MailboxControlEntrySize;
}
Formats::Bytes MakeMailboxControl(Mailbox mailbox);
bool IsMailboxControl(Formats::ByteView bytes, Mailbox mailbox);

}
