#pragma once

#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"

#include <memory>
#include <optional>
#include <string_view>

namespace Channels
{

// The water that the scenes of the Wii Speak Channel lie on, and the drops that fall on it.
//
// The chat room has its own water (02000_bg of the scene talk) with its moving waves. Settings and Create Message lie on the water of
// the menus (01020_bg of system.szs) in one of its five colors (0x8006E568, 0x8006EA50), and a drop falls on it every 30 steps
// (0x800A6600). A scene without water of its own, as Choose Miis, lies on the water of the scene it came from (CHN-95).
class WiiSpeakWater
{
public:
    enum class MenuColor
    {
        Letter,
        Settings,
    };

    // The water of the chat room, whose drops fall when dropsFall is true.
    static std::unique_ptr<WiiSpeakWater> OfTheChatRoom(WiiSpeakSession& session, bool dropsFall);

    static std::unique_ptr<WiiSpeakWater> OfTheMenus(WiiSpeakSession& session, MenuColor color);

    WiiSpeakWater(WiiSpeakSession& session, std::unique_ptr<SceneLayout> layout, std::string_view dropName,
        std::optional<int> stepsBetweenDrops);

    // Once in every step: the waves and the drops.
    void Update();

    // The water only; the ripples on it are drawn by the screen, between the water and its buttons.
    void Draw();

    SceneLayout& Layout();

private:
    WiiSpeakSession& session;
    std::unique_ptr<SceneLayout> layout;
    std::string_view dropName;
    std::optional<int> stepsBetweenDrops;
    int stepsSinceTheLastDrop = 0;
};

}
