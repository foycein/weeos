#include "SystemMenu/WiiMenu/IconScripts.h"

#include "SystemMenu/WiiMenu/ChannelTitleIds.h"
#include "SystemMenu/WiiMenu/LayoutParts.h"

#include <array>
#include <format>
#include <string_view>

namespace SystemMenu
{

namespace
{

constexpr std::string_view IconLayout = "icon";

// The Shop's script shows the two pictures of the title "Wii Shop Channel" in the language of the console (setRegionPanes);
// the menu speaks English.
constexpr std::array<std::string_view, 2> ShopTitlesInEnglish = {"P_title_E_00", "P_title_E_01"};

// Without recommendations the Shop's script plays only the start of animation 0, frames 0 to 629 (LEN_START_ANIMATION).
constexpr float ShopStartAnimationEnd = 630;

// The Forecast's script hides the marks of the weather, which only a forecast fills.
constexpr std::string_view ForecastWeatherMarks = "code";

// The News script empties this hidden text, in which it keeps the number of the news it shows.
constexpr std::string_view NewsShownNewsNumber = "send_id";

// The animations of the scripts, by their numbers: icon_Rso0 of the Photo Channel brings its default photos, icon_Rso0 of the Shop
// its bag and title, icon_Rso0 of the Forecast Channel its logo of weathernews; icon_Rso1 and icon_Rso2 of the News Channel move
// its line of headlines and the belts behind it.
constexpr int PhotoDefaultPhotosAnimation = 0;
constexpr int PhotoPostedPhotoAnimation = 1;
constexpr int ShopBagAndTitleAnimation = 0;
constexpr int ForecastLogoAnimation = 0;
constexpr int NewsHeadlinesAnimation = 1;
constexpr int NewsBeltsAnimation = 2;

constexpr std::string_view PostedPhotoTexture = "plate1.tpl";
constexpr std::string_view PostedPhotoPane = "pic_photo";

// The pane of the posted photo is 1 x 1 in the file; the script gives it the size of the photo on the cork. That size was not read
// from the byte code: a 4:3 photo as high as the middle photo of the default animation, 55 units (Assumed, CHN-33).
constexpr float PostedPhotoHeight = 55;
constexpr float PostedPhotoWidth = PostedPhotoHeight * 4 / 3;

// Photo.icon: without a photo posted to the Wii Menu (noerase/photo_i.dat), the first run starts animation 0: the three photos
// come one after the other, the belt with the logo follows, everything fades away and it begins again. With a posted photo it starts
// animation 1, which shows the pane pic_photo with that photo.
void RunPhotoChannelScript(MenuLayout& icon, ScriptAnimations& animations, bool hasPostedPhoto)
{
    if (!hasPostedPhoto)
    {
        animations.Start(PhotoDefaultPhotosAnimation);
        return;
    }

    Formats::Pane& photo = icon.PaneNamed(PostedPhotoPane);
    photo.width = PostedPhotoWidth;
    photo.height = PostedPhotoHeight;
    animations.Start(PhotoPostedPhotoAnimation);
}

// The Shop's script ([shopping]): without recommendations (RcmdData.bin, a WiiConnect24 download) it shows the title of the
// language, puts every animation on its first frame and plays the start of animation 0 as a loop: the shopping bag and the title
// fade in over the sliding background and away again.
void RunShopChannelScript(MenuLayout& icon, ScriptAnimations& animations)
{
    for (std::string_view titlePane : ShopTitlesInEnglish)
        icon.PaneNamed(titlePane).isVisible = true;

    for (int number = 0; number < ScriptAnimationCount; ++number)
        animations.ShowFirstFrame(number);

    animations.StartWithEnd(ShopBagAndTitleAnimation, ShopStartAnimationEnd);
}

// Forecast.icon: without a forecast (wc24dl.vff, a WiiConnect24 download), the marks of the weather stay hidden and the first run
// starts animation 0: the title shows, then the logo of weathernews fades in and away again.
void RunForecastChannelScript(MenuLayout& icon, ScriptAnimations& animations)
{
    icon.PaneNamed(ForecastWeatherMarks).isVisible = false;
    animations.Start(ForecastLogoAnimation);
}

// News.icon: without news, the script empties its hidden number and puts animations 1 and 2 on their first frame: the headlines
// wait right of the icon and the belts stay clear. The map moves with the icon's own icon_Start.
void RunNewsChannelScript(MenuLayout& icon, ScriptAnimations& animations)
{
    icon.SetText(NewsShownNewsNumber, u"");
    animations.ShowFirstFrame(NewsHeadlinesAnimation);
    animations.ShowFirstFrame(NewsBeltsAnimation);
}

}

IconScript FindIconScript(Ios::TitleId titleId, bool isIconDrivenByCode)
{
    if (!isIconDrivenByCode)
        return IconScript::None;

    if (IsInFamily(titleId, SystemChannelKind, PhotoChannel11Family))
        return IconScript::PhotoChannel;

    if (IsInFamily(titleId, SystemChannelKind, ShopChannelFamily))
        return IconScript::ShopChannel;

    if (IsInFamily(titleId, SystemChannelKind, ForecastChannelFamily))
        return IconScript::ForecastChannel;

    if (IsInFamily(titleId, SystemChannelKind, NewsChannelFamily))
        return IconScript::NewsChannel;

    return IconScript::None;
}

std::string ScriptAnimationName(int number)
{
    return std::format("icon_Rso{}", number);
}

std::string ScriptGroupName(int number)
{
    return std::format("Rso{}", number);
}

ScriptAnimations::ScriptAnimations(MenuLayout& icon, const Layout::LayoutArchive& archive, const std::vector<int>& numbers)
{
    Formats::Layout file = archive.ReadLayout(std::string(IconLayout));
    for (int number : numbers)
    {
        std::string animationName = ScriptAnimationName(number);
        std::string groupName = ScriptGroupName(number);
        if (FindTopGroup(file, groupName))
            players[number] = std::make_unique<FramePlayer>(icon, archive, animationName, icon.Live().PanesOfGroup(groupName));
        else
            players[number] = std::make_unique<FramePlayer>(icon, archive, animationName);

        isLoopByNumber[number] = ModeOfAnimationFile(archive, animationName) == Layout::PlaybackMode::Loop;
    }
}

// A loop of the menu ends after the last frame of its file, an animation that plays once on it (0x81369ABC).
void ScriptAnimations::Start(int number)
{
    FramePlayer* player = Find(number);
    if (!player)
        return;

    float lastFrame = player->LastFrameOfTheFile();
    float endFrame = isLoopByNumber[number] ? lastFrame + 1 : lastFrame;
    StartWithEnd(number, endFrame);
}

void ScriptAnimations::StartWithEnd(int number, float endFrame)
{
    FramePlayer* player = Find(number);
    if (!player)
        return;

    if (isLoopByNumber[number])
        player->Loop(0, endFrame);
    else
        player->PlayOnce(0, endFrame);
}

void ScriptAnimations::ShowFirstFrame(int number)
{
    FramePlayer* player = Find(number);
    if (player)
        player->Show(0);
}

void ScriptAnimations::Step()
{
    for (auto& [number, player] : players)
        player->Step();
}

FramePlayer* ScriptAnimations::Find(int number)
{
    auto found = players.find(number);
    if (found == players.end())
        return nullptr;

    return found->second.get();
}

void RunIconScript(IconScript script, MenuLayout& icon, ScriptAnimations& animations, bool hasPostedPhoto)
{
    switch (script)
    {
    case IconScript::PhotoChannel:
        RunPhotoChannelScript(icon, animations, hasPostedPhoto);
        break;

    case IconScript::ShopChannel:
        RunShopChannelScript(icon, animations);
        break;

    case IconScript::ForecastChannel:
        RunForecastChannelScript(icon, animations);
        break;

    case IconScript::NewsChannel:
        RunNewsChannelScript(icon, animations);
        break;

    case IconScript::None:
        break;
    }
}

PostedPhotoPictures::PostedPhotoPictures(const Layout::LayoutArchive& iconArchive, Formats::RgbaImage postedPhoto)
    : archive(iconArchive)
    , photo(std::move(postedPhoto))
{
}

std::optional<Formats::RgbaImage> PostedPhotoPictures::FindPicture(const std::string& textureFileName) const
{
    if (textureFileName == PostedPhotoTexture)
        return photo;

    return archive.FindPicture(textureFileName);
}

}
