#pragma once

#include "Channels/ChannelServices.h"
#include "Channels/Manual/PageLayout.h"

#include "Formats/U8Archive.h"
#include "Graphics/GraphicsDevice.h"
#include "Layout/TextFonts.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace Channels
{

// The sounds of a manual, by their names in the channel's sound archive.
struct ManualSounds
{
    std::string_view focus;
    std::string_view select;
    std::string_view scroll;
    std::string_view close;
};

// Where a channel keeps its manual: the archive of its pages in a content of its title, and the page the manual starts with.
struct ManualLocation
{
    std::uint16_t contentIndex;
    std::string_view archivePath;
    std::string_view startPage;
    ManualSounds sounds;
};

// The electronic manual of a channel, its "Operations Guide" (docs/audit/OpenPoints/Channels.md CHN-13): pages of HTML that the
// channel's browser shows. WeeOS lays them out natively (PageLayout) and draws them with the console's bitmap font.
//  - The pointer over a link or a button runs its onmouseover: the pictures and backgrounds change and the focus sound plays.
//  - A on a link runs its onclick and opens its page. A new page comes in over 15 steps: from the side for Back and Next
//    (ext.LeftScroll, ext.RightScroll), else the old page fades away (the Forecast's 0x800809B8).
//  - Up and down on the +Control Pad scroll a page that is higher than the screen, HOME closes the manual.
class ManualViewer
{
public:
    // The services must outlive the viewer. Throws a ChannelError when the start page is not in the archive.
    ManualViewer(ChannelServices& channelServices, Formats::U8Archive pages, std::string_view startPage, const ManualSounds& manualSounds);
    ~ManualViewer();

    ManualViewer(const ManualViewer&) = delete;
    ManualViewer& operator=(const ManualViewer&) = delete;

    void Update();
    void Draw();

    // HOME was pressed: the channel system fades the manual out and the channel in.
    bool WantsToClose() const;

    const LaidOutPage& ShownPage() const;
    bool IsChangingPage() const;
    float ScrollPosition() const;

    // A point of the screen picture, in layout units, as a point of the shown page.
    Formats::Vector2 PictureToPage(Formats::Vector2 picturePoint) const;

private:
    struct ManualPicture
    {
        Graphics::TextureHandle texture;
        float width = 0;
        float height = 0;
    };

    enum class PageChange
    {
        Fade,
        FromLeft,
        FromRight,
    };

    const ManualPicture* PictureOf(const std::string& path);
    void OpenPage(const std::string& path, PageChange pageChange);
    void RunActions(const std::vector<PageAction>& actions);
    void FollowPointer();
    void PressAtPointer();
    void Scroll();
    std::optional<Formats::Vector2> PointedPagePlace() const;

    void ChooseScrollingArea();
    float LowestScrollPosition() const;
    bool IsScrolledLink(std::size_t linkIndex) const;

    void DrawPage(const LaidOutPage& shownPage, float offsetX, float scroll);
    void DrawParts(const LaidOutPage& shownPage, std::size_t first, std::size_t end, float offsetX, float offsetY);
    void DrawPart(const PagePart& part, float offsetX, float offsetY);
    void DrawText(const PagePart& part, float offsetX, float offsetY);
    void DrawPicture(const ManualPicture& picture, const PageRectangle& place, bool isTiled, float offsetX, float offsetY, std::uint8_t alpha);
    void DrawLeavingPage();
    Graphics::TextureHandle CapturePage(const LaidOutPage& shownPage);
    Formats::Vector2 PageToPicture(float pageX, float pageY) const;

    ChannelServices& services;
    Graphics::GraphicsDevice& device;
    const Layout::DrawableFont& font;
    ManualSounds sounds;
    Formats::U8Archive pageArchive;
    PageFiles files;
    std::map<std::string, ManualPicture, std::less<>> pictures;

    LaidOutPage page;
    std::map<std::string, std::string> swappedPictures;
    std::map<std::string, std::string> swappedBackgrounds;
    std::set<std::size_t> linksUnderPointer;

    // The page that is leaving, as a picture, and how far the change has come.
    std::optional<Graphics::TextureHandle> leavingPage;
    PageChange change = PageChange::Fade;
    int changeStep = 0;

    // The frame that scrolls, or the whole page; nothing when everything fits.
    std::optional<PageFrameArea> scrollingArea;
    float scrollPosition = 0;
    float scrollTarget = 0;
    float scrollSpeed = 0;
    bool wantsToClose = false;
};

// Reads the archive of a channel's manual pages from its title and opens the viewer on the start page.
std::unique_ptr<ManualViewer> OpenManual(ChannelServices& services, const ManualLocation& location);

}
