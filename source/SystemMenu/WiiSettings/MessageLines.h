#pragma once

#include <string>
#include <vector>

namespace SystemMenu
{

// The lines of a message of the menu. A message holds its line breaks as line feeds.
inline std::vector<std::u16string> LinesOfMessage(const std::u16string& message)
{
    std::vector<std::u16string> lines;
    std::size_t lineStart = 0;
    for (std::size_t position = 0; position <= message.size(); ++position)
    {
        bool isEndOfLine = position == message.size() || message[position] == u'\n';
        if (!isEndOfLine)
            continue;

        if (position > lineStart)
            lines.push_back(message.substr(lineStart, position - lineStart));

        lineStart = position + 1;
    }

    return lines;
}

}
