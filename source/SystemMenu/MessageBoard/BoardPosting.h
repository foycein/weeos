#pragma once

#include "SystemMenu/MessageBoard/MessageStore.h"

#include "Formats/Vectors.h"

#include <optional>
#include <string>

namespace SystemMenu
{

using MiiData = std::array<Formats::Byte, Formats::BoardMessageMiiSize>;

// A new letter lies at a random place of the board: x from -230 to 230 and y from 180 down to -80, the box in which a letter can be
// dragged (0x81342670 with the limits of 0x81332BE0). The two numbers are random numbers from 0 up to 1.
Formats::Vector2 PlaceForNewLetter(float firstRandomNumber, float secondRandomNumber);

// The board message of a memo, as the import of the mail service of the original makes it from the memo that "Write a memo" sends to
// the console itself (0x8140C92C, 0x81342670): the kind Memo, from this console, cannot be answered, no subject, the text as the body,
// the Mii when one was added. WeeOS has no mail service, so the memo goes straight into the database.
Formats::BoardMessage MakeMemo(const std::u16string& text, const std::optional<MiiData>& mii, Formats::Vector2 place, const LocalTime& now);

// Puts a memo on a day of the board, at the time of day of "now", as the original dates a memo with the day that the board shows.
StoredMessage PostMemo(MessageStore& store, BoardDay day, const Formats::BoardMessage& memo, const LocalTime& now);

// A picture that a channel of this console posts to the board, as the Photo Channel does: a letter from this console with the JPEG
// file as its first attachment, in the folder "odh", on the day of "now".
StoredMessage PostPicture(MessageStore& store, const Formats::Bytes& jpegFile, Formats::Vector2 place, const LocalTime& now);

}
