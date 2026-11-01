#include "Channels/WiiSpeak/WiiSpeakWater.h"

namespace Channels
{

namespace
{

const std::string ChatRoomWaves = "02000_bg_wait_00";

// The original lets a drop fall on the chat room's water with every third drop of the water's sound (0x800A9DE4); WeeOS does not play
// the events of that sound, so a drop falls every 150 steps (EFF-14, Assumed).
constexpr int ChatRoomStepsBetweenDrops = 150;
constexpr int MenuStepsBetweenDrops = 30;

}

std::unique_ptr<WiiSpeakWater> WiiSpeakWater::OfTheChatRoom(WiiSpeakSession& session, bool dropsFall)
{
    auto layout = std::make_unique<SceneLayout>(session.services.display, session.files.SceneLayouts("talk", "02000_bg.arc"), "02000_bg");
    std::size_t wavesSlot = layout->Layout().BindAnimationToPane(ChatRoomWaves, "RootPane", Layout::PlaybackMode::Loop);
    layout->Layout().StartAnimation(wavesSlot);

    std::optional<int> stepsBetweenDrops;
    if (dropsFall)
        stepsBetweenDrops = ChatRoomStepsBetweenDrops;
    return std::make_unique<WiiSpeakWater>(session, std::move(layout), WiiSpeakRipples::ChatRoomDrop, stepsBetweenDrops);
}

std::unique_ptr<WiiSpeakWater> WiiSpeakWater::OfTheMenus(WiiSpeakSession& session, MenuColor color)
{
    auto layout = std::make_unique<SceneLayout>(session.services.display, session.files.SystemLayouts("01020_bg.arc"), "01020_bg");
    layout->PlayAnimation(color == MenuColor::Settings ? "color_02" : "color_01");
    return std::make_unique<WiiSpeakWater>(session, std::move(layout), WiiSpeakRipples::MenuDrop, MenuStepsBetweenDrops);
}

WiiSpeakWater::WiiSpeakWater(WiiSpeakSession& waterSession, std::unique_ptr<SceneLayout> waterLayout, std::string_view drop,
    std::optional<int> steps)
    : session(waterSession)
    , layout(std::move(waterLayout))
    , dropName(drop)
    , stepsBetweenDrops(steps)
{
}

void WiiSpeakWater::Update()
{
    layout->Calculate();
    if (!stepsBetweenDrops)
        return;

    ++stepsSinceTheLastDrop;
    if (stepsSinceTheLastDrop < *stepsBetweenDrops)
        return;

    stepsSinceTheLastDrop = 0;
    session.ripples.StartDrop(dropName);
}

void WiiSpeakWater::Draw()
{
    layout->Draw();
}

SceneLayout& WiiSpeakWater::Layout()
{
    return *layout;
}

}
