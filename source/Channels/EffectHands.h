#pragma once

#include "Effects/Effect.h"
#include "Effects/EffectResources.h"
#include "Effects/ParticleDrawing.h"

#include "SystemMenu/Display.h"
#include "SystemMenu/InputManager.h"

#include "Formats/Bytes.h"

#include <array>
#include <memory>

namespace Channels
{

// How a channel lays its particle hands onto the picture.
struct EffectHandsLook
{
    // Every player's hand (the Forecast and the News), or only the first player's (the Mii Channel).
    bool showsEveryPlayer = true;

    // The units of the channel's orthographic projection for its hands, laid onto the canvas: canvas = box * scale.
    Formats::Vector2 boxToCanvasScale = {1, 1};

    // Where the shadow lies from the hand, in the units of the box.
    Formats::Vector2 shadowOffset = {0, 0};

    // How far the closed hand rises above the pointer, in the units of the box (the Mii Channel holds a Mii under it).
    float holdingLift = 0;

    // The Forecast turns every hand with the remote, the Mii Channel only the pointing one (0x80029264).
    bool closedHandTurns = true;
};

// The hands of the channels that draw them as particle effects of nw4r_defcursor_all01: the hand "def_cursor_normal_1p"
// (hold, open; 1p to 4p) and under it its shadow "def_cursor_normal_sd", placed where the remote points and turned with the remote.
// The effects make one particle that lives one step, so a hand is made again in every step at the pointer, as the Forecast does
// (0x8002C304).
class EffectHands
{
public:
    // The display must outlive the hands.
    EffectHands(SystemMenu::Display& display, Formats::ByteView effectFile, Formats::ByteView textureFile, const EffectHandsLook& look);

    // The closed hand instead of the pointing one, as while the Mii Channel holds a Mii.
    void SetHolding(int playerIndex, bool isHolding);

    // Once in every step: makes the hands of the players whose remotes point at the picture.
    void Update(const SystemMenu::InputManager& input);

    void Draw();

    // The number of hands shown in this step, for the tests.
    int ShownHandCount() const;

private:
    struct Hand
    {
        bool isHolding = false;
        std::unique_ptr<Effects::Effect> shadow;
        std::unique_ptr<Effects::Effect> pointer;
    };

    void UpdateHand(int playerIndex, const std::optional<SystemMenu::PlayerInput>& playerInput);
    std::unique_ptr<Effects::Effect> MakeEffect(const std::string& name, Formats::Vector2 position, float angle);

    SystemMenu::Display& display;
    Effects::EffectResources resources;
    EffectHandsLook look;
    Effects::EffectCamera camera;
    std::array<Hand, SystemMenu::PlayerCount> hands;
    std::uint16_t nextSeed = 1;
};

}
