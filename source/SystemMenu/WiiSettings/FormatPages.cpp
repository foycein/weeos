#include "SystemMenu/WiiSettings/FormatPages.h"

#include "SystemMenu/WiiSettings/PageBuilder.h"

namespace SystemMenu
{

namespace
{

// The page that works shows its message for at least this many steps (about two seconds), so that the user can read it.
constexpr int StepsOfWorking = 120;

constexpr std::u16string_view Title = u"Format Wii System Memory";

}

FormatPages::FormatPages(PageNavigator& pageNavigator, Settings::SystemMemoryFormatter& systemMemoryFormatter)
    : navigator(pageNavigator)
    , formatter(systemMemoryFormatter)
{
}

bool FormatPages::Owns(PageId page) const
{
    switch (page)
    {
    case PageId::FormatWarning:
    case PageId::FormatAccountWarning:
    case PageId::FormatConfirm:
    case PageId::FormatWorking:
    case PageId::FormatDone:
    case PageId::FormatFailed:
        return true;

    default:
        return false;
    }
}

void FormatPages::EnterPage(PageId page)
{
    if (page == PageId::FormatWorking)
    {
        stepsWorked = 0;
        hasFormatted = false;
    }
}

void FormatPages::BuildPage(PageId page, Page& content)
{
    switch (page)
    {
    case PageId::FormatWarning:
        BuildWarning(content, PageId::FormatAccountWarning,
            {u"All added channels and save", u"data will be erased. Once", u"erased, it can never be", u"recovered. Even channels that",
                u"have been copied to an", u"SD Card can't be restored.", u"Do you still want to format?"});
        break;

    case PageId::FormatAccountWarning:
        BuildWarning(content, PageId::FormatConfirm,
            {u"If you use the Wii Shop", u"Channel, it is recommended", u"that you remove your", u"Wii Shop Channel Account",
                u"before reformatting your", u"Wii System Memory. Do you", u"still want to reformat?"});
        break;

    case PageId::FormatConfirm:
        BuildConfirm(content);
        break;

    case PageId::FormatWorking:
        BuildWorking(content);
        break;

    case PageId::FormatDone:
        BuildDone(content);
        break;

    default:
        BuildFailed(content);
        break;
    }
}

// The first two questions: the left button cancels and the right one goes on.
void FormatPages::BuildWarning(Page& content, PageId nextPage, std::vector<std::u16string> lines)
{
    PageBuilder builder(content);
    builder.SetTitle(1, std::u16string(Title));
    builder.AddMessage(std::move(lines));

    builder.AddBottomLeft(u"Cancel", [this] { navigator.GoTo(PageId::ThirdIndex); }).sound = PageSound::Back;
    builder.AddBottomRight(u"Format", [this, nextPage] { navigator.GoTo(nextPage); });
}

// The last question has the buttons the other way round: the left one formats, the right one says no.
void FormatPages::BuildConfirm(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, std::u16string(Title));
    builder.AddMessage({u"Remember, all of your", u"current data will be", u"permanently lost. Are you", u"sure you want to format",
        u"the Wii System Memory?"});

    builder.AddBottomLeft(u"Format", [this] { navigator.GoTo(PageId::FormatWorking); });
    builder.AddBottomRight(u"No", [this] { navigator.GoTo(PageId::ThirdIndex); }).sound = PageSound::Back;
}

void FormatPages::BuildWorking(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, std::u16string(Title));
    builder.AddMessage({u"Formatting the", u"Wii System Memory..."});
}

void FormatPages::BuildDone(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, std::u16string(Title));
    builder.AddMessage({u"The Wii System Memory", u"has been formatted.", u"The Wii console", u"will now shut down."});

    builder.AddBottomRight(u"OK", [this] { formatter.ShutDownConsole(); });
}

void FormatPages::BuildFailed(Page& content)
{
    PageBuilder builder(content);
    builder.SetTitle(1, std::u16string(Title));
    builder.AddMessage({u"The Wii System Memory", u"could not be formatted."});

    builder.AddBottomRight(u"OK", [this] { navigator.GoTo(PageId::ThirdIndex); });
}

// The format is made in the first step of the page that works. The page stays for a little while after the work is done.
void FormatPages::Step(PageId shownPage)
{
    if (shownPage != PageId::FormatWorking)
        return;

    if (!hasFormatted)
    {
        hasFormatted = true;
        try
        {
            formatter.Format();
        }
        catch (const std::exception&)
        {
            navigator.GoTo(PageId::FormatFailed);
            return;
        }
    }

    ++stepsWorked;
    if (stepsWorked >= StepsOfWorking)
        navigator.GoTo(PageId::FormatDone);
}

}
