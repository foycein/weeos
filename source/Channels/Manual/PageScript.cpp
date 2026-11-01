#include "Channels/Manual/PageScript.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>

namespace Channels
{

namespace
{

// Dreamweaver's functions for pictures that change under the pointer, which the pages copy into their scripts.
constexpr std::string_view SwapPictureFunction = "MM_swapImage";
constexpr std::string_view RestorePicturesFunction = "MM_swapImgRestore";

// MM_swapImage(name, unused, picture, preload): the name and the picture of the swap.
constexpr std::size_t SwapNameArgument = 0;
constexpr std::size_t SwapPictureArgument = 2;

// The calls of the host's object that a function body may make, and what each one does.
struct HostCall
{
    std::string_view call;
    PageActionKind kind;
};

constexpr std::array<HostCall, 4> HostCalls = {{
    {"ext.FocusSound(", PageActionKind::PlayFocusSound},
    {"ext.SelectSound(", PageActionKind::PlaySelectSound},
    {"ext.LeftScroll(", PageActionKind::SlideFromLeft},
    {"ext.RightScroll(", PageActionKind::SlideFromRight},
}};

constexpr std::string_view LocationChange = "location.href";
constexpr std::string_view BackgroundChange = "style.backgroundImage";
constexpr std::string_view ElementLookup = "getElementById(";
constexpr std::string_view UrlStart = "url(";
constexpr std::string_view FunctionKeyword = "function ";

std::string_view Trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);

    return text;
}

bool IsQuote(char character)
{
    return character == '\'' || character == '"';
}

std::string_view WithoutQuotes(std::string_view text)
{
    text = Trim(text);
    if (text.size() >= 2 && IsQuote(text.front()) && text.back() == text.front())
        return text.substr(1, text.size() - 2);

    return text;
}

// The parts of a text between the separators, where a separator inside quotes does not count.
std::vector<std::string_view> SplitOutsideQuotes(std::string_view text, std::string_view separators)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    char openQuote = 0;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        char character = text[index];
        if (openQuote != 0)
        {
            if (character == openQuote)
                openQuote = 0;
            continue;
        }

        if (IsQuote(character))
            openQuote = character;
        else if (separators.find(character) != std::string_view::npos)
        {
            parts.push_back(text.substr(start, index - start));
            start = index + 1;
        }
    }
    parts.push_back(text.substr(start));
    return parts;
}

// The text between the bracket at open and the bracket that closes it.
std::string_view InsideBrackets(std::string_view text, std::size_t open, char openBracket, char closeBracket)
{
    int depth = 0;
    for (std::size_t index = open; index < text.size(); ++index)
    {
        if (text[index] == openBracket)
            ++depth;

        if (text[index] != closeBracket)
            continue;

        --depth;
        if (depth == 0)
            return text.substr(open + 1, index - open - 1);
    }
    return text.substr(std::min(open + 1, text.size()));
}

// The value of a word of a function body: an argument when it is a parameter, else the text itself without quotes.
std::string ValueOf(std::string_view word, const std::vector<std::string>& parameters, const std::vector<std::string>& arguments)
{
    word = Trim(word);
    for (std::size_t index = 0; index < parameters.size() && index < arguments.size(); ++index)
    {
        if (parameters[index] == word)
            return arguments[index];
    }
    return std::string(WithoutQuotes(word));
}

}

void PageScript::AddScript(std::string_view code)
{
    std::size_t position = code.find(FunctionKeyword);
    while (position != std::string_view::npos)
    {
        std::size_t nameStart = position + FunctionKeyword.size();
        std::size_t open = code.find('(', nameStart);
        std::size_t bodyOpen = code.find('{', open);
        if (open == std::string_view::npos || bodyOpen == std::string_view::npos)
            return;

        std::string name(Trim(code.substr(nameStart, open - nameStart)));
        ScriptFunction function;
        for (std::string_view parameter : SplitOutsideQuotes(InsideBrackets(code, open, '(', ')'), ","))
        {
            if (!Trim(parameter).empty())
                function.parameters.emplace_back(Trim(parameter));
        }
        function.body = std::string(InsideBrackets(code, bodyOpen, '{', '}'));
        functions[name] = std::move(function);

        position = code.find(FunctionKeyword, bodyOpen + 1);
    }
}

std::vector<PageAction> PageScript::ActionsOf(std::string_view handler) const
{
    std::vector<PageAction> actions;
    for (std::string_view statement : SplitOutsideQuotes(handler, ";"))
    {
        statement = Trim(statement);
        std::size_t open = statement.find('(');
        if (open == std::string_view::npos)
            continue;

        std::vector<std::string> arguments;
        for (std::string_view argument : SplitOutsideQuotes(InsideBrackets(statement, open, '(', ')'), ","))
            arguments.emplace_back(WithoutQuotes(argument));

        ActionsOfCall(Trim(statement.substr(0, open)), arguments, actions);
    }
    return actions;
}

void PageScript::ActionsOfCall(std::string_view name, const std::vector<std::string>& arguments, std::vector<PageAction>& actions) const
{
    if (name == SwapPictureFunction && arguments.size() > SwapPictureArgument)
    {
        actions.push_back({PageActionKind::SwapPicture, arguments[SwapNameArgument], arguments[SwapPictureArgument]});
        return;
    }

    if (name == RestorePicturesFunction)
    {
        actions.push_back({PageActionKind::RestorePictures, {}, {}});
        return;
    }

    auto function = functions.find(name);
    if (function != functions.end())
        ActionsOfBody(function->second, arguments, actions);
}

// The pages' own functions do one of a few things: call the host (ext.FocusSound()), change the address of the window
// (parent.location.href = url) or change the background of an element (getElementById(name).style.backgroundImage = "url(...)").
void PageScript::ActionsOfBody(const ScriptFunction& function, const std::vector<std::string>& arguments, std::vector<PageAction>& actions) const
{
    for (std::string_view statement : SplitOutsideQuotes(function.body, ";\n"))
    {
        for (const HostCall& hostCall : HostCalls)
        {
            if (statement.find(hostCall.call) != std::string_view::npos)
                actions.push_back({hostCall.kind, {}, {}});
        }

        std::size_t equals = statement.find('=');
        if (equals == std::string_view::npos)
            continue;

        std::string_view target = statement.substr(0, equals);
        std::string_view value = statement.substr(equals + 1);
        if (target.find(LocationChange) != std::string_view::npos)
            actions.push_back({PageActionKind::OpenPage, {}, ValueOf(value, function.parameters, arguments)});

        std::size_t lookup = target.find(ElementLookup);
        std::size_t url = value.find(UrlStart);
        bool changesBackground = target.find(BackgroundChange) != std::string_view::npos;
        bool namesElementAndPicture = lookup != std::string_view::npos && url != std::string_view::npos;
        if (changesBackground && namesElementAndPicture)
        {
            std::string_view elementWord = InsideBrackets(target, lookup + ElementLookup.size() - 1, '(', ')');
            std::string_view path = InsideBrackets(value, url + UrlStart.size() - 1, '(', ')');
            std::string elementName = ValueOf(elementWord, function.parameters, arguments);
            actions.push_back({PageActionKind::SwapBackground, elementName, std::string(WithoutQuotes(path))});
        }
    }
}

}
