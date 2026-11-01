#include "Channels/Manual/ManualViewer.h"

#include "Channels/ChannelError.h"
#include "Channels/ChannelFiles.h"

#include "Formats/GifImage.h"
#include "Formats/PngImage.h"
#include "Text/LaidOutText.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Channels
{

namespace
{

// The browser of the channel shows the pages on the whole 16:9 screen, 832 by 456 pixels; a pixel is a layout unit.
constexpr float ScreenWidth = 832;
constexpr float ScreenHeight = 456;

// A captured page is drawn at twice the size of the screen, so it stays sharp on a large window.
constexpr std::uint32_t CaptureScale = 2;

// A new page takes 15 steps to come in; with Back and Next the pages move by 15 percent of the screen (0x800809B8).
constexpr int StepsOfPageChange = 15;
constexpr float SlideOfPageChange = 0.15f;

// Scrolling (0x800809B8): the speed grows by 2 pixels a step up to 40 and falls so that the page stops on its place.
constexpr float ScrollAcceleration = 2;
constexpr float FastestScroll = 40;

// A page this close to its place stops there.
constexpr float StoppingDistance = 1;

// How far the place to scroll to moves while the +Control Pad is held (Assumed: the key handling of the browser was not read).
constexpr float ScrollPerStep = 8;

// A bold text is drawn twice, the second time this much to the right: the bitmap font has no bold glyphs.
constexpr float BoldOffset = 1;

constexpr std::uint8_t Opaque = 255;
constexpr Formats::Rgba8Color White = {255, 255, 255, Opaque};
constexpr char16_t NoBreakSpace = u' ';

constexpr SystemMenu::ButtonSet HomeButtons = {Input::RemoteButtons::Home, Input::ClassicButtons::Home};
constexpr SystemMenu::ButtonSet ScrollUpButtons = {Input::RemoteButtons::Up, Input::ClassicButtons::Up};
constexpr SystemMenu::ButtonSet ScrollDownButtons = {Input::RemoteButtons::Down, Input::ClassicButtons::Down};

bool EndsWith(std::string_view text, std::string_view ending)
{
    return text.size() >= ending.size() && text.substr(text.size() - ending.size()) == ending;
}

std::optional<Formats::RgbaImage> ReadPicture(std::string_view path, Formats::ByteView bytes)
{
    if (EndsWith(path, ".png"))
        return Formats::ReadPngImage(bytes);
    if (EndsWith(path, ".gif"))
        return Formats::ReadGifImage(bytes);

    return std::nullopt;
}

bool IsHeld(const SystemMenu::InputManager& input, const SystemMenu::ButtonSet& buttons)
{
    for (int playerIndex = 0; playerIndex < SystemMenu::PlayerCount; ++playerIndex)
    {
        const std::optional<SystemMenu::PlayerInput>& player = input.Player(playerIndex);
        if (player && (player->state.buttonsHeld & buttons.remoteButtons) != 0)
            return true;
    }
    return false;
}

}

ManualViewer::ManualViewer(ChannelServices& channelServices, Formats::U8Archive pages, std::string_view startPage, const ManualSounds& manualSounds)
    : services(channelServices)
    , device(channelServices.display.Device())
    , font(channelServices.systemFonts.Find("wbf1.brfna"))
    , sounds(manualSounds)
    , pageArchive(std::move(pages))
{
    files.readFile = [this](std::string_view path) { return pageArchive.FindFile(path); };
    files.pictureSize = [this](std::string_view path) -> std::optional<Formats::Vector2> {
        const ManualPicture* picture = PictureOf(std::string(path));
        if (picture == nullptr)
            return std::nullopt;

        return Formats::Vector2{picture->width, picture->height};
    };

    page = LayOutPage(files, startPage, font.layouter, ScreenWidth, ScreenHeight);
    ChooseScrollingArea();
}

ManualViewer::~ManualViewer()
{
    for (auto& [path, picture] : pictures)
        device.DestroyTexture(picture.texture);

    if (leavingPage)
        device.DestroyTexture(*leavingPage);
}

void ManualViewer::Update()
{
    if (leavingPage)
        ++changeStep;

    if (leavingPage && changeStep >= StepsOfPageChange)
    {
        device.DestroyTexture(*leavingPage);
        leavingPage.reset();
    }

    if (wantsToClose)
        return;

    if (services.input.AnyPlayerPressed(HomeButtons))
    {
        services.sounds.Request(sounds.close);
        wantsToClose = true;
        return;
    }

    FollowPointer();
    if (services.input.AnyPlayerPressed(SystemMenu::SelectButtons) && !leavingPage)
        PressAtPointer();

    Scroll();
}

void ManualViewer::Draw()
{
    float changeShare = static_cast<float>(changeStep) / StepsOfPageChange;
    float slide = 0;
    if (leavingPage && change == PageChange::FromLeft)
        slide = -(1 - changeShare) * SlideOfPageChange * ScreenWidth;
    if (leavingPage && change == PageChange::FromRight)
        slide = (1 - changeShare) * SlideOfPageChange * ScreenWidth;

    DrawPage(page, slide, scrollPosition);
    DrawLeavingPage();
}

bool ManualViewer::WantsToClose() const
{
    return wantsToClose;
}

const LaidOutPage& ManualViewer::ShownPage() const
{
    return page;
}

bool ManualViewer::IsChangingPage() const
{
    return leavingPage.has_value();
}

float ManualViewer::ScrollPosition() const
{
    return scrollPosition;
}

// The point on the screen, without the scrolling: the links of the scrolling area are moved by it when they are looked for.
Formats::Vector2 ManualViewer::PictureToPage(Formats::Vector2 picturePoint) const
{
    return {picturePoint.x + ScreenWidth / 2, ScreenHeight / 2 - picturePoint.y};
}

const ManualViewer::ManualPicture* ManualViewer::PictureOf(const std::string& path)
{
    auto found = pictures.find(path);
    if (found != pictures.end())
        return &found->second;

    std::optional<Formats::ByteView> bytes = pageArchive.FindFile(path);
    if (!bytes)
        return nullptr;

    std::optional<Formats::RgbaImage> image = ReadPicture(path, *bytes);
    if (!image)
        return nullptr;

    Graphics::TextureDescription description;
    description.width = image->width;
    description.height = image->height;
    description.pixels = image->pixels;
    description.horizontalWrap = Graphics::WrapMode::Repeat;
    description.verticalWrap = Graphics::WrapMode::Repeat;

    ManualPicture picture;
    picture.texture = device.CreateTexture(description);
    picture.width = static_cast<float>(image->width);
    picture.height = static_cast<float>(image->height);
    return &pictures.emplace(path, picture).first->second;
}

void ManualViewer::OpenPage(const std::string& path, PageChange pageChange)
{
    if (leavingPage)
        device.DestroyTexture(*leavingPage);

    leavingPage = CapturePage(page);
    change = pageChange;
    changeStep = 0;

    page = LayOutPage(files, path, font.layouter, ScreenWidth, ScreenHeight);
    ChooseScrollingArea();
    swappedPictures.clear();
    swappedBackgrounds.clear();
    linksUnderPointer.clear();
    scrollPosition = 0;
    scrollTarget = 0;
    scrollSpeed = 0;
}

void ManualViewer::RunActions(const std::vector<PageAction>& actions)
{
    PageChange pageChange = PageChange::Fade;
    for (const PageAction& action : actions)
    {
        switch (action.kind)
        {
        case PageActionKind::OpenPage:
            OpenPage(action.path, pageChange);
            return;

        case PageActionKind::SwapPicture:
            swappedPictures[action.elementName] = action.path;
            break;

        case PageActionKind::RestorePictures:
            swappedPictures.clear();
            break;

        case PageActionKind::SwapBackground:
            swappedBackgrounds[action.elementName] = action.path;
            break;

        case PageActionKind::PlayFocusSound:
            services.sounds.Request(sounds.focus);
            break;

        case PageActionKind::PlaySelectSound:
            services.sounds.Request(sounds.select);
            break;

        case PageActionKind::SlideFromLeft:
            pageChange = PageChange::FromLeft;
            services.sounds.Request(sounds.select);
            break;

        case PageActionKind::SlideFromRight:
            pageChange = PageChange::FromRight;
            services.sounds.Request(sounds.select);
            break;
        }
    }
}

// A link that the pointer comes to runs its onmouseover, one that it leaves its onmouseout.
void ManualViewer::FollowPointer()
{
    std::optional<Formats::Vector2> pointed = PointedPagePlace();
    std::set<std::size_t> linksNow;
    for (std::size_t link = 0; pointed && link < page.links.size(); ++link)
    {
        Formats::Vector2 placeOnPage = *pointed;
        if (IsScrolledLink(link))
        {
            if (!scrollingArea->place.Contains(*pointed))
                continue;
            placeOnPage.y += scrollPosition;
        }

        if (page.links[link].place.Contains(placeOnPage))
            linksNow.insert(link);
    }

    for (std::size_t link : linksUnderPointer)
    {
        if (!linksNow.contains(link))
            RunActions(page.links[link].whenPointerLeaves);
    }
    for (std::size_t link : linksNow)
    {
        if (!linksUnderPointer.contains(link))
            RunActions(page.links[link].whenPointerComes);
    }
    linksUnderPointer = linksNow;
}

// A press runs the onclick of every link under the pointer (a button and the picture inside it can both have one); the first page
// that one of them opens is shown.
void ManualViewer::PressAtPointer()
{
    std::vector<PageAction> actions;
    for (std::size_t link : linksUnderPointer)
    {
        const std::vector<PageAction>& pressed = page.links[link].whenPressed;
        actions.insert(actions.end(), pressed.begin(), pressed.end());
    }

    std::stable_partition(actions.begin(), actions.end(), [](const PageAction& action) { return action.kind != PageActionKind::OpenPage; });
    RunActions(actions);
}

// The frame whose content is higher than the frame scrolls; a page without frames scrolls as a whole.
void ManualViewer::ChooseScrollingArea()
{
    scrollingArea.reset();
    if (page.frames.empty())
    {
        PageFrameArea wholePage;
        wholePage.place = {0, 0, ScreenWidth, ScreenHeight};
        wholePage.contentHeight = page.height;
        wholePage.endPart = page.parts.size();
        wholePage.endLink = page.links.size();
        if (page.height > ScreenHeight)
            scrollingArea = wholePage;
        return;
    }

    for (const PageFrameArea& frame : page.frames)
    {
        if (frame.contentHeight > frame.place.height)
        {
            scrollingArea = frame;
            return;
        }
    }
}

float ManualViewer::LowestScrollPosition() const
{
    if (!scrollingArea)
        return 0;

    return scrollingArea->contentHeight - scrollingArea->place.height;
}

bool ManualViewer::IsScrolledLink(std::size_t linkIndex) const
{
    return scrollingArea && linkIndex >= scrollingArea->firstLink && linkIndex < scrollingArea->endLink;
}

void ManualViewer::Scroll()
{
    float lowest = LowestScrollPosition();
    float previousTarget = scrollTarget;
    if (IsHeld(services.input, ScrollUpButtons))
        scrollTarget -= ScrollPerStep;
    if (IsHeld(services.input, ScrollDownButtons))
        scrollTarget += ScrollPerStep;
    scrollTarget = std::clamp(scrollTarget, 0.0f, lowest);
    if (scrollTarget != previousTarget && scrollSpeed == 0)
        services.sounds.Request(sounds.scroll);

    // Faster while the page could still stop in time, then as fast as lets it stop on the place (v = 2 (sqrt(d + 1/4) - 1/2)).
    float distance = scrollTarget - scrollPosition;
    if (std::abs(distance) <= StoppingDistance)
    {
        scrollPosition = scrollTarget;
        scrollSpeed = 0;
        return;
    }

    float direction = distance > 0 ? 1.0f : -1.0f;
    float speed = std::abs(scrollSpeed);
    float stepsToStop = speed / ScrollAcceleration;
    float distanceToStop = stepsToStop * (stepsToStop + 1);
    if (std::abs(distance) > distanceToStop)
        speed = std::min(speed + ScrollAcceleration, FastestScroll);
    else
        speed = ScrollAcceleration * (std::sqrt(std::abs(distance) + 0.25f) - 0.5f);

    scrollSpeed = direction * speed;
    scrollPosition += scrollSpeed;
}

std::optional<Formats::Vector2> ManualViewer::PointedPagePlace() const
{
    for (int playerIndex = 0; playerIndex < SystemMenu::PlayerCount; ++playerIndex)
    {
        const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(playerIndex);
        if (player && player->state.pointerIsValid)
            return PictureToPage(services.display.PointerToCanvas(player->state.pointer));
    }
    return std::nullopt;
}

// A page without a background of its own is white, as the browser's pages are. The parts of the scrolling area move up by the
// scrolling and are cut at the edges of the area.
void ManualViewer::DrawPage(const LaidOutPage& shownPage, float offsetX, float scroll)
{
    PagePart background;
    background.kind = PagePartKind::Fill;
    background.place = {0, 0, ScreenWidth, ScreenHeight};
    background.color = White;
    DrawPart(background, offsetX, 0);

    if (!scrollingArea)
    {
        DrawParts(shownPage, 0, shownPage.parts.size(), offsetX, 0);
        return;
    }

    DrawParts(shownPage, 0, scrollingArea->firstPart, offsetX, 0);

    const PageRectangle& area = scrollingArea->place;
    Formats::Vector2 topLeft = PageToPicture(area.left + offsetX, area.top);
    Formats::Vector2 bottomRight = PageToPicture(area.left + area.width + offsetX, area.top + area.height);
    device.SetScissor({topLeft.x, bottomRight.x, bottomRight.y, topLeft.y});
    DrawParts(shownPage, scrollingArea->firstPart, scrollingArea->endPart, offsetX, -scroll);
    device.ClearScissor();

    DrawParts(shownPage, scrollingArea->endPart, shownPage.parts.size(), offsetX, 0);
}

void ManualViewer::DrawParts(const LaidOutPage& shownPage, std::size_t first, std::size_t end, float offsetX, float offsetY)
{
    for (std::size_t index = first; index < end && index < shownPage.parts.size(); ++index)
        DrawPart(shownPage.parts[index], offsetX, offsetY);
}

void ManualViewer::DrawPart(const PagePart& part, float offsetX, float offsetY)
{
    if (part.kind == PagePartKind::Text)
    {
        DrawText(part, offsetX, offsetY);
        return;
    }

    if (part.kind == PagePartKind::Fill)
    {
        Formats::Vector2 topLeft = PageToPicture(part.place.left + offsetX, part.place.top + offsetY);
        Formats::Vector2 bottomRight = PageToPicture(part.place.left + part.place.width + offsetX, part.place.top + part.place.height + offsetY);
        Graphics::Quad quad{};
        quad.positions = {topLeft, {bottomRight.x, topLeft.y}, bottomRight, {topLeft.x, bottomRight.y}};
        quad.colors = {part.color, part.color, part.color, part.color};
        device.DrawQuad(quad, Graphics::Material());
        return;
    }

    std::string path = part.path;
    const std::map<std::string, std::string>& swaps = part.isTiled ? swappedBackgrounds : swappedPictures;
    auto swapped = swaps.find(part.elementName);
    if (!part.elementName.empty() && swapped != swaps.end())
        path = swapped->second;

    const ManualPicture* picture = PictureOf(path);
    if (picture != nullptr)
        DrawPicture(*picture, part.place, part.isTiled, offsetX, offsetY, Opaque);
}

void ManualViewer::DrawText(const PagePart& part, float offsetX, float offsetY)
{
    std::u16string text = part.text;
    std::replace(text.begin(), text.end(), NoBreakSpace, u' ');

    Text::TextStyle style;
    style.fontSize = {part.look.size, part.look.size};
    style.characterSpace = part.look.letterSpacing;
    style.topColor = part.look.color;
    style.bottomColor = part.look.color;
    style.alignment = {Text::HorizontalAlignment::Left, Text::VerticalAlignment::Top};

    int copies = part.look.isBold ? 2 : 1;
    for (int copy = 0; copy < copies; ++copy)
    {
        Formats::Vector2 topLeft = PageToPicture(part.place.left + offsetX + BoldOffset * static_cast<float>(copy), part.place.top + offsetY);
        Text::TextArea area;
        area.left = topLeft.x;
        area.top = topLeft.y;
        area.width = part.place.width + part.look.size;
        area.height = part.place.height;
        font.drawer.Draw(font.layouter.LayOut(text, style, area));
    }
}

void ManualViewer::DrawPicture(const ManualPicture& picture, const PageRectangle& place, bool isTiled, float offsetX, float offsetY, std::uint8_t alpha)
{
    Formats::Vector2 topLeft = PageToPicture(place.left + offsetX, place.top + offsetY);
    Formats::Vector2 bottomRight = PageToPicture(place.left + place.width + offsetX, place.top + place.height + offsetY);
    float right = isTiled ? place.width / picture.width : 1;
    float bottom = isTiled ? place.height / picture.height : 1;

    Formats::Rgba8Color color = White;
    color.alpha = alpha;

    Graphics::Quad quad{};
    quad.positions = {topLeft, {bottomRight.x, topLeft.y}, bottomRight, {topLeft.x, bottomRight.y}};
    quad.colors = {color, color, color, color};
    quad.textureCoordinates[0] = {Formats::Vector2{0, 0}, {right, 0}, {right, bottom}, {0, bottom}};

    Graphics::Material material;
    material.textures.push_back({picture.texture, Graphics::TextureMatrix()});
    device.DrawQuad(quad, material);
}

// The page that leaves fades away over the new one, and moves aside with Back and Next.
void ManualViewer::DrawLeavingPage()
{
    if (!leavingPage)
        return;

    float changeShare = static_cast<float>(changeStep) / StepsOfPageChange;
    float slide = 0;
    if (change == PageChange::FromLeft)
        slide = changeShare * SlideOfPageChange * ScreenWidth;
    if (change == PageChange::FromRight)
        slide = -changeShare * SlideOfPageChange * ScreenWidth;

    ManualPicture capture;
    capture.texture = *leavingPage;
    capture.width = ScreenWidth;
    capture.height = ScreenHeight;
    auto alpha = static_cast<std::uint8_t>(Opaque * (1 - changeShare));
    DrawPicture(capture, {0, 0, ScreenWidth, ScreenHeight}, false, slide, 0, alpha);
}

Graphics::TextureHandle ManualViewer::CapturePage(const LaidOutPage& shownPage)
{
    Graphics::TextureHandle capture = device.BeginOffscreen({ScreenWidth, ScreenHeight},
        static_cast<std::uint32_t>(ScreenWidth) * CaptureScale, static_cast<std::uint32_t>(ScreenHeight) * CaptureScale);
    DrawPage(shownPage, 0, scrollPosition);
    device.EndOffscreen();
    return capture;
}

Formats::Vector2 ManualViewer::PageToPicture(float pageX, float pageY) const
{
    return {pageX - ScreenWidth / 2, ScreenHeight / 2 - pageY};
}

std::unique_ptr<ManualViewer> OpenManual(ChannelServices& services, const ManualLocation& location)
{
    Formats::U8Archive content = ReadArchiveContent(services.contents, location.contentIndex);
    Formats::U8Archive pages = Formats::ReadU8Archive(FindFileOrFail(content, location.archivePath));
    return std::make_unique<ManualViewer>(services, std::move(pages), location.startPage, location.sounds);
}

}
