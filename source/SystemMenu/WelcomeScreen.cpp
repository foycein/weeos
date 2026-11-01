#include "SystemMenu/WelcomeScreen.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/FrontPanel.h"
#include "SystemMenu/MenuClock.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/WeeOSVersion.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

// The welcome fades in and out in as many steps as the warning of the original (it_Has_a_SeenIn and it_Has_a_SeenOut).
constexpr int StepsOfFade = 29;

// "Press A" starts this long after the welcome has faded in, and blinks: clear, opaque after half of its steps, clear again.
constexpr double SecondsBeforeNotice = 1;
constexpr int StepsOfBlink = 60;

constexpr int Opaque = 255;

// The calm white and blue of the Wii: a white that turns into a pale blue towards the bottom, the blue of the Wii for the logo and
// "Press A", and gray text.
constexpr Formats::Rgba8Color BackgroundTop = {255, 255, 255, Opaque};
constexpr Formats::Rgba8Color BackgroundBottom = {222, 236, 247, Opaque};
constexpr Formats::Rgba8Color WiiBlue = {52, 190, 237, Opaque};
constexpr Formats::Rgba8Color TextGray = {85, 85, 85, Opaque};
constexpr Formats::Rgba8Color QuietGray = {140, 140, 140, Opaque};

// The places on the page of 608 by 456 points in the middle of the picture, from the top.
constexpr PageRectangle LogoBounds = {0, 36, PageWidth, 96};
constexpr PageRectangle LogoLineBounds = {224, 134, 160, 4};
constexpr PageRectangle VersionBounds = {0, 146, PageWidth, 24};
constexpr PageRectangle DescriptionBounds = {0, 186, PageWidth, 60};
constexpr PageRectangle AuthorBounds = {0, 256, PageWidth, 30};
constexpr PageRectangle ContactBounds = {0, 288, PageWidth, 26};
constexpr PageRectangle BreakBounds = {0, 330, PageWidth, 26};
constexpr PageRectangle NoticeBounds = {0, 380, PageWidth, 44};

constexpr float LogoSize = 92;
constexpr float VersionSize = 20;
constexpr float DescriptionSize = 24;
constexpr float AuthorSize = 26;
constexpr float ContactSize = 21;
constexpr float BreakSize = 21;
constexpr float NoticeSize = 34;

constexpr std::u16string_view Logo = u"WeeOS";
constexpr std::u16string_view Description = u"A hand-written reconstruction\nof the Wii system for the PC";
constexpr std::u16string_view Author = u"Made by foycein";
constexpr std::u16string_view Contact = u"Discord: foycein     GitHub: foycein";
constexpr std::u16string_view BreakReminder = u"Playing is fun, and so is a break now and then.";
constexpr std::u16string_view Notice = u"Press A";

Formats::Rgba8Color WithOpacity(Formats::Rgba8Color color, int opacity)
{
    color.alpha = static_cast<std::uint8_t>(color.alpha * opacity / Opaque);
    return color;
}

// Clear at the start of a blink, opaque in its middle, clear at its end.
int OpacityOfBlink(int stepsOfNotice)
{
    int stepInBlink = stepsOfNotice % StepsOfBlink;
    int halfBlink = StepsOfBlink / 2;
    if (stepInBlink <= halfBlink)
        return Opaque * stepInBlink / halfBlink;

    return Opaque * (StepsOfBlink - stepInBlink) / halfBlink;
}

}

WelcomeScreen::WelcomeScreen(MenuServices& menuServices)
    : Screen(ScreenId::HealthAndSafety)
    , services(menuServices)
    , continueWatcher(menuServices.input, menuServices.clock)
{
}

void WelcomeScreen::Build()
{
    font = std::make_unique<PageFont>(services.display.Device(), services.files);

    services.fader.SetClear();
    services.fader.Calculate();
    services.cursors.SetVisible(false);
}

void WelcomeScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main)
        return;

    int opacity = WelcomeOpacity();
    DrawBackground(opacity);
    DrawText(Logo, LogoBounds, LogoSize, WiiBlue, opacity);
    DrawLogoLine(opacity);
    DrawText(u"Version " + std::u16string(WeeOSVersion), VersionBounds, VersionSize, QuietGray, opacity);
    DrawText(Description, DescriptionBounds, DescriptionSize, TextGray, opacity);
    DrawText(Author, AuthorBounds, AuthorSize, TextGray, opacity);
    DrawText(Contact, ContactBounds, ContactSize, QuietGray, opacity);
    DrawText(BreakReminder, BreakBounds, BreakSize, TextGray, opacity);

    std::optional<int> noticeOpacity = NoticeOpacity();
    if (noticeOpacity)
        DrawText(Notice, NoticeBounds, NoticeSize, WiiBlue, *noticeOpacity);
}

bool WelcomeScreen::DrawsWithChildren() const
{
    return true;
}

int WelcomeScreen::WelcomeOpacity() const
{
    if (isFadingOut)
        return Opaque * std::max(StepsOfFade - stepsOfFadeOut, 0) / StepsOfFade;

    return Opaque * std::min(stepsOfFadeIn, StepsOfFade) / StepsOfFade;
}

std::optional<int> WelcomeScreen::NoticeOpacity() const
{
    if (!isNoticeShown)
        return std::nullopt;

    return OpacityOfBlink(stepsOfNotice) * WelcomeOpacity() / Opaque;
}

// The screen waits until the welcome has faded in and a second has passed, then "Press A" shows and the screen is ready. The fade in
// counts as done one step after its last step, as the animation of the original's warning does.
bool WelcomeScreen::IsReady()
{
    ++stepsOfFadeIn;
    if (!hasFadedIn && stepsOfFadeIn > StepsOfFade)
    {
        hasFadedIn = true;
        noticeWaitBeganSeconds = services.clock.Seconds();
    }

    bool hasWaitedForTheNotice = hasFadedIn && services.clock.Seconds() - noticeWaitBeganSeconds > SecondsBeforeNotice;
    if (!hasWaitedForTheNotice)
        return false;

    isNoticeShown = true;
    continueWatcher.Begin();
    return true;
}

bool WelcomeScreen::RunMain()
{
    ++stepsOfNotice;

    bool isDone = continueWatcher.HasUserConfirmed();
    if (isDone)
    {
        services.sounds.Request(SoundName::ButtonPushed);
        isFadingOut = true;
    }

    return isDone;
}

bool WelcomeScreen::RunClosing()
{
    ++stepsOfNotice;
    ++stepsOfFadeOut;

    if (stepsOfFadeOut >= StepsOfFade)
        services.fader.StartFadeOut();

    if (services.fader.GetStatus() != FaderStatus::Covered)
        return false;

    services.cursors.SetVisible(true);
    services.frontPanel.NoteMenuReached();
    GoToRoot(ScreenId::MainFrame, 0);
    return true;
}

// The background reaches the edges of a picture of any width.
void WelcomeScreen::DrawBackground(int opacity)
{
    Graphics::Canvas canvas = services.display.GetCanvas();
    float left = -canvas.width / 2;
    float right = canvas.width / 2;
    float top = canvas.height / 2;
    float bottom = -canvas.height / 2;

    Formats::Rgba8Color topColor = WithOpacity(BackgroundTop, opacity);
    Formats::Rgba8Color bottomColor = WithOpacity(BackgroundBottom, opacity);

    Graphics::Quad quad;
    quad.positions = {{{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
    quad.colors = {{topColor, topColor, bottomColor, bottomColor}};
    services.display.Device().DrawQuad(quad, Graphics::Material());
}

// A short blue line under the logo, which fades out towards both of its ends.
void WelcomeScreen::DrawLogoLine(int opacity)
{
    float left = static_cast<float>(LogoLineBounds.left - PageWidth / 2);
    float middle = left + static_cast<float>(LogoLineBounds.width) / 2;
    float right = left + static_cast<float>(LogoLineBounds.width);
    float top = static_cast<float>(PageHeight / 2 - LogoLineBounds.top);
    float bottom = top - static_cast<float>(LogoLineBounds.height);

    Formats::Rgba8Color end = WithOpacity(WiiBlue, 0);
    Formats::Rgba8Color center = WithOpacity(WiiBlue, opacity);

    Graphics::Quad leftHalf;
    leftHalf.positions = {{{left, top}, {middle, top}, {middle, bottom}, {left, bottom}}};
    leftHalf.colors = {{end, center, center, end}};
    services.display.Device().DrawQuad(leftHalf, Graphics::Material());

    Graphics::Quad rightHalf;
    rightHalf.positions = {{{middle, top}, {right, top}, {right, bottom}, {middle, bottom}}};
    rightHalf.colors = {{center, end, end, center}};
    services.display.Device().DrawQuad(rightHalf, Graphics::Material());
}

void WelcomeScreen::DrawText(std::u16string_view text, const PageRectangle& bounds, float size, Formats::Rgba8Color color, int opacity)
{
    Text::TextAlignment centered;
    font->Draw(std::u16string(text), bounds, centered, WithOpacity(color, opacity), size);
}

}
