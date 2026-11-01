#include "SystemMenu/MessageBoard/PlayLog.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

constexpr int MinutesPerHour = 60;
constexpr int SecondsPerMinute = 60;
constexpr int LongestShownMinutes = 23 * MinutesPerHour + 59;
constexpr int ShortestShownMinutes = 1;
constexpr int DigitsBase = 10;

constexpr std::u16string_view BlankLine = u"\n\n";
constexpr std::u16string_view LineBreak = u"\n";
constexpr std::u16string_view TimeIndent = u"\n     ";

int WholeMinutesOfTicks(std::uint64_t ticks)
{
    return static_cast<int>(ticks / ConsoleTicksPerSecond / SecondsPerMinute);
}

char16_t Digit(int value)
{
    return static_cast<char16_t>(u'0' + value % DigitsBase);
}

// "HH:MM": two digits of the hours, the separator of the language, two digits of the minutes (0x81340B18).
std::u16string MakeTimeText(int minutes, const Text::MessageCatalog& messages)
{
    int hours = minutes / MinutesPerHour;
    int minutesOfTheHour = minutes % MinutesPerHour;

    std::u16string text;
    text.push_back(Digit(hours / DigitsBase));
    text.push_back(Digit(hours));
    text += messages.Find(PlayLogMessage::TimeSeparator);
    text.push_back(Digit(minutesOfTheHour / DigitsBase));
    text.push_back(Digit(minutesOfTheHour));
    return text;
}

// The name of a title has two lines of 21 characters; the second line is written only when it has something.
std::u16string NameLinesOf(const Formats::PlayRecord& play)
{
    auto firstLineStart = play.name.begin();
    auto secondLineStart = firstLineStart + Formats::PlayRecordNameLineLength;
    std::u16string firstLine(firstLineStart, std::find(firstLineStart, secondLineStart, u'\0'));
    if (*secondLineStart == u'\0')
        return firstLine;

    std::u16string secondLine(secondLineStart, std::find(secondLineStart, play.name.end(), u'\0'));
    return firstLine + std::u16string(LineBreak) + secondLine;
}

std::optional<StoredMessage> FindPlayLogOfDay(MessageStore& store, BoardDay day)
{
    for (StoredMessage& stored : store.MessagesOfDay(day))
    {
        bool isPlayLog = stored.place.kindFolder == KindFolder::PlayLog && stored.message.kind == Formats::BoardMessageKind::PlayLog;
        if (isPlayLog && stored.message.attachments[0] && stored.message.attachments[0]->kind == Formats::BoardAttachmentKind::PlayLog)
            return stored;
    }
    return std::nullopt;
}

void AddPlay(Formats::PlayLogAttachment& playLog, const Formats::PlayRecord& play)
{
    for (std::optional<Formats::PlayRecord>& slot : playLog.plays)
    {
        if (!slot)
        {
            slot = play;
            return;
        }
    }
    playLog.ticksOfOtherTitles += play.lastTime - play.startTime;
}

Formats::BoardMessage MakeNewPlayLog(const Formats::PlayLogAttachment& playLog, const Text::MessageCatalog& messages, const LocalTime& now)
{
    Formats::BoardMessage message;
    message.kind = Formats::BoardMessageKind::PlayLog;
    message.boardTime = TicksOf(now);
    message.senderKind = Formats::BoardSenderKind::ThisConsole;
    message.cannotBeAnswered = 1;
    message.subject = messages.Find(PlayLogMessage::Title);
    message.body = MakePlayLogText(playLog, messages);
    message.attachments[0] = Formats::BoardAttachment{Formats::BoardAttachmentKind::PlayLog, Formats::WritePlayLogAttachment(playLog)};
    return message;
}

}

int MinutesShownForPlay(const Formats::PlayRecord& play)
{
    int minutes = WholeMinutesOfTicks(play.lastTime - play.startTime);
    return std::clamp(minutes, ShortestShownMinutes, LongestShownMinutes);
}

std::u16string MakePlayLogText(const Formats::PlayLogAttachment& playLog, const Text::MessageCatalog& messages)
{
    std::u16string text = messages.Find(PlayLogMessage::PlayHistory);
    int totalMinutes = 0;
    for (const std::optional<Formats::PlayRecord>& play : playLog.plays)
    {
        if (!play)
            continue;

        int minutes = MinutesShownForPlay(*play);
        totalMinutes += minutes;
        text += BlankLine;
        text += NameLinesOf(*play);
        text += TimeIndent;
        text += MakeTimeText(minutes, messages);
    }

    if (playLog.ticksOfOtherTitles != 0)
    {
        int minutes = std::max(WholeMinutesOfTicks(playLog.ticksOfOtherTitles), ShortestShownMinutes);
        totalMinutes += minutes;
        text += BlankLine;
        text += messages.Find(PlayLogMessage::Others);
        text += TimeIndent;
        text += MakeTimeText(minutes, messages);
    }

    text += BlankLine;
    text += messages.Find(PlayLogMessage::TotalPlayTime);
    text += TimeIndent;
    text += MakeTimeText(totalMinutes, messages);
    return text;
}

void PutPlayOnTheBoard(MessageStore& store, const Formats::PlayRecord& play, const Text::MessageCatalog& messages, const LocalTime& now)
{
    LocalTime endOfThePlay = TimeOfSeconds(SecondsSince2000(TimeOfTicks(play.lastTime)));
    std::optional<StoredMessage> existing = FindPlayLogOfDay(store, DayOf(endOfThePlay));
    if (existing)
    {
        Formats::PlayLogAttachment playLog = Formats::ReadPlayLogAttachment(existing->message.attachments[0]->contents);
        AddPlay(playLog, play);
        existing->message.body = MakePlayLogText(playLog, messages);
        existing->message.attachments[0]->contents = Formats::WritePlayLogAttachment(playLog);
        store.ChangeMessage(*existing, now);
        return;
    }

    Formats::PlayLogAttachment playLog;
    AddPlay(playLog, play);
    store.AddMessage(Formats::PlayLogRecordTypeName, KindFolder::PlayLog, endOfThePlay, MakeNewPlayLog(playLog, messages, now), now);
}

}
