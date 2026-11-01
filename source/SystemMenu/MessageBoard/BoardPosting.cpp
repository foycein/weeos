#include "SystemMenu/MessageBoard/BoardPosting.h"

namespace SystemMenu
{

namespace
{

constexpr float LeftEdgeOfTheBox = -230;
constexpr float WidthOfTheBox = 460;
constexpr float TopEdgeOfTheBox = 180;
constexpr float HeightOfTheBox = 260;

}

Formats::Vector2 PlaceForNewLetter(float firstRandomNumber, float secondRandomNumber)
{
    return Formats::Vector2{LeftEdgeOfTheBox + WidthOfTheBox * firstRandomNumber, TopEdgeOfTheBox - HeightOfTheBox * secondRandomNumber};
}

Formats::BoardMessage MakeMemo(const std::u16string& text, const std::optional<MiiData>& mii, Formats::Vector2 place, const LocalTime& now)
{
    Formats::BoardMessage memo;
    memo.positionX = place.x;
    memo.positionY = place.y;
    memo.kind = Formats::BoardMessageKind::Memo;
    memo.boardTime = TicksOf(now);
    memo.senderKind = Formats::BoardSenderKind::ThisConsole;
    memo.cannotBeAnswered = 1;
    memo.body = text;
    memo.mii = mii;
    return memo;
}

StoredMessage PostMemo(MessageStore& store, BoardDay day, const Formats::BoardMessage& memo, const LocalTime& now)
{
    return store.AddMessage(Formats::BoardRecordTypeName, KindFolder::Text, AtTimeOfDay(day, now), memo, now);
}

StoredMessage PostPicture(MessageStore& store, const Formats::Bytes& jpegFile, Formats::Vector2 place, const LocalTime& now)
{
    Formats::BoardMessage letter;
    letter.positionX = place.x;
    letter.positionY = place.y;
    letter.kind = Formats::BoardMessageKind::Letter;
    letter.boardTime = TicksOf(now);
    letter.senderKind = Formats::BoardSenderKind::ThisConsole;
    letter.cannotBeAnswered = 1;
    letter.attachments[0] = Formats::BoardAttachment{Formats::BoardAttachmentKind::Picture, jpegFile};
    return store.AddMessage(Formats::BoardRecordTypeName, KindFolder::Picture, now, letter, now);
}

}
