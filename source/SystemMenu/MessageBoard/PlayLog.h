#pragma once

#include "SystemMenu/BackgroundServices/NandFileAccess.h"
#include "SystemMenu/MessageBoard/MessageStore.h"

#include "Formats/PlayLogAttachment.h"
#include "Formats/PlayRecordFile.h"
#include "Text/MessageCatalog.h"

#include <string>

namespace SystemMenu
{

// The message numbers of the play log.
namespace PlayLogMessage
{
constexpr std::uint16_t Title = 136;
constexpr std::uint16_t PlayHistory = 132;
constexpr std::uint16_t TotalPlayTime = 135;
constexpr std::uint16_t Others = 137;
constexpr std::uint16_t TimeSeparator = 138;
}

// The time a play shows: the whole minutes it ran, at least one, at most 23 hours and 59 minutes (0x81340DEC).
int MinutesShownForPlay(const Formats::PlayRecord& play);

// "Today's Play History", every title of the day with its time ("00:26"), the other titles and the total (0x81340DEC, 0x81340B18):
// the text of a play log as the menu writes it.
std::u16string MakePlayLogText(const Formats::PlayLogAttachment& playLog, const Text::MessageCatalog& messages);

// "Today's Accomplishments" (iplPlayTimeLog.cpp, 0x813406CC): the record of the title that ran last goes into the play log of the
// day on which it ended. The play takes the first free place of that log, or adds its time to the other titles when the ten places
// are full; a day without a log gets a new one in the middle of the board. "now" is the time of the console.
void PutPlayOnTheBoard(MessageStore& store, const Formats::PlayRecord& play, const Text::MessageCatalog& messages, const LocalTime& now);

// At the start of the menu the record of the title that ran last goes onto the board and its file is deleted, as the original's thread
// of the mail service calls the play log when it starts (0x81357650 calls 0x813406CC). Nothing happens without a good record.
void PutTheLastPlayOnTheBoard(NandFileAccess& files, MessageStore& store, const Text::MessageCatalog& messages, const LocalTime& now);

}
