#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace Formats
{

constexpr std::size_t BoardMessageHeaderSize = 0x148;
constexpr std::size_t BoardMessageSenderSize = 0x100;
constexpr std::size_t BoardMessageMiiSize = 0x4A;
constexpr std::size_t BoardMessageAttachmentCount = 2;

// What a message on the Message Board is. It chooses the small letter and the view of the letter.
enum class BoardMessageKind : Byte
{
    // Written on this console with "Write a memo".
    Memo = 0,

    // Came from another console, the internet or a channel of this console (a picture of the Photo Channel).
    Letter = 1,

    // "Today's Accomplishments", the play log of a day.
    PlayLog = 2,

    // A notice that asks for a system update.
    UpdateNotice = 3,
};

// Who sent the message.
enum class BoardSenderKind : std::uint16_t
{
    ThisConsole = 0,
    AnotherWii = 1,
    InternetMail = 2,
};

// The kinds of the two attachments.
enum class BoardAttachmentKind : std::uint32_t
{
    // A JPEG picture.
    Picture = 1,

    // A U8 archive with stationery, a thumbnail, a channel jump or a sound.
    Archive = 2,

    // The play log of a day (Formats/PlayLogAttachment.h).
    PlayLog = 3,

    // Data for a channel.
    ChannelData = 4,
};

struct BoardAttachment
{
    BoardAttachmentKind kind = BoardAttachmentKind::Picture;
    Bytes contents;
};

// A message of the Message Board: the data of a CDB record of the type "ripl_board_record" or "playtimelog" (Formats/CdbRecordFile.h).
// The menu builds it in 0x8133F560: a header of 0x148 bytes that starts with "RI_5" and ends with the CRC-32 of its first 0x140 bytes,
// then the texts in UTF-16, the Mii and the attachments. Numbers are stored with the most significant byte first.
struct BoardMessage
{
    // Where the small letter lies on the board, in the units of the layout.
    float positionX = 0;
    float positionY = 0;

    // 1 when the sender lets the user opt out of its mail. The two bytes after it are kept as they were read.
    Byte optOutFlag = 0;
    std::array<Byte, 2> unusedKindBytes{};
    BoardMessageKind kind = BoardMessageKind::Memo;

    // When the message was put on the board: ticks of the console's timer (a quarter of the bus clock) since the year 2000.
    std::uint64_t boardTime = 0;

    // Who sent it: the first eight bytes are the Wii number of the sender, or the whole block is an e-mail address.
    std::array<Byte, BoardMessageSenderSize> sender{};
    BoardSenderKind senderKind = BoardSenderKind::ThisConsole;

    // 1 when the message cannot be answered.
    std::uint16_t cannotBeAnswered = 0;

    // The name of the sender (the menu shows it as the title of a letter) and the text. Both may be empty.
    std::u16string subject;
    std::u16string body;

    // The Mii of the sender, as the face library stores it.
    std::optional<std::array<Byte, BoardMessageMiiSize>> mii;

    std::array<std::optional<BoardAttachment>, BoardMessageAttachmentCount> attachments;
};

// Throws a FormatError when the magic, the CRC or a place inside the message is wrong.
BoardMessage ReadBoardMessage(ByteView bytes);

// Lays the parts out as the menu does: each text takes its characters, a zero and room to a multiple of 8 bytes; the Mii takes 0x4C
// bytes; the attachments start at a multiple of 0x20 and each takes a multiple of 0x20 bytes.
Bytes WriteBoardMessage(const BoardMessage& message);

}
