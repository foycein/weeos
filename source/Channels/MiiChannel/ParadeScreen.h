#pragma once

#include "Channels/ChannelServices.h"
#include "Channels/MiiChannel/ButtonFocus.h"
#include "Channels/MiiChannel/MessageWindow.h"
#include "Channels/MiiChannel/MiiChannelData.h"
#include "Channels/MiiChannel/MiiEffects.h"
#include "Channels/MiiChannel/MiiFigures.h"
#include "Channels/MiiChannel/MiiPlaza.h"
#include "Channels/MiiChannel/PlazaGround.h"

#include "SystemMenu/MenuLayout.h"

#include <memory>
#include <optional>
#include <string_view>

namespace Channels
{

// The Mii Parade (uraNigaoe_00 over the road, the table at 80241FC4): the Miis of other consoles, and the Miis this plaza sent, walking
// along the road toward the camera. A Mii dropped on "Send to Mii Plaza" goes back to the plaza, one dropped on "Erase" leaves the
// parade; "Speed" makes them run; "Travel Settings" needs WiiConnect24; "Mii Plaza" goes back; + and - zoom as on the plaza. An empty
// parade says that nobody is here.
class ParadeScreen
{
public:
    static constexpr std::string_view TravelSettingsButton = "squButtonNull_00";
    static constexpr std::string_view SpeedButton = "squButtonNull_01";
    static constexpr std::string_view SendToPlazaButton = "squButtonNull_06";
    static constexpr std::string_view PlazaButton = "squButtonNull_07";
    static constexpr std::string_view EraseButton = "squButtonNull_08";

    // Everything given must outlive the screen.
    ParadeScreen(ChannelServices& channelServices, const Layout::LayoutArchive& layouts, const Formats::NumberedMessageFile& messages,
        const MiiFigures& miiFigures, const PlazaGround& paradeRoad, const Mii::MiiResources& faceParts, MessageWindow& messageWindow,
        MiiChannelData& channelData, MiiEffects& miiEffects, std::uint32_t randomSeed);

    // The Miis of the parade come onto the road; without any, the parade says that nobody is here.
    void Start();

    // True once "Mii Plaza" was pressed; the channel then shows the plaza again.
    bool TakeWayBackToThePlaza();

    const MiiPlaza& Parade() const;
    Formats::Vector2 PictureCenterOf(std::size_t place) const;
    Formats::Vector2 PlaceOf(std::string_view paneName) const;

    void Update(bool acceptsInput);
    void Draw();

private:
    void ShowParadeMiis();
    void HandleButtons();
    void HandleHand();
    void HandleMessageAnswer();
    void DropHeldMii(Formats::Vector2 pointer);
    void ShowSpeedIcon();
    bool IsPressed(std::string_view buttonPane) const;
    Graphics::Canvas Canvas() const;

    ChannelServices& services;
    const MiiFigures& figures;
    MiiEffects& effects;
    const PlazaGround& road;
    MessageWindow& message;
    MiiChannelData& data;
    std::unique_ptr<SystemMenu::MenuLayout> layout;
    std::unique_ptr<ButtonFocus> focus;
    MiiHeads heads;
    MiiPlaza parade;

    std::optional<std::size_t> miiToErase;
    bool isWayBackAsked = false;
};

}
