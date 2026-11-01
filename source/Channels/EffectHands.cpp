#include "Channels/EffectHands.h"

#include <format>
#include <numbers>

namespace Channels
{

namespace
{

constexpr float RadiansPerDegree = std::numbers::pi_v<float> / 180;

std::string HandName(bool isHolding, int playerIndex)
{
    return std::format("def_cursor_{}_{}p", isHolding ? "hold" : "normal", playerIndex + 1);
}

std::string ShadowName(bool isHolding)
{
    return std::format("def_cursor_{}_sd", isHolding ? "hold" : "normal");
}

}

EffectHands::EffectHands(SystemMenu::Display& handDisplay, Formats::ByteView effectFile, Formats::ByteView textureFile,
    const EffectHandsLook& handsLook)
    : display(handDisplay)
    , resources(handDisplay.Device())
    , look(handsLook)
{
    resources.AddFiles(effectFile, textureFile);
    camera.boxToCanvasScale = look.boxToCanvasScale;
}

void EffectHands::SetHolding(int playerIndex, bool isHolding)
{
    hands.at(static_cast<std::size_t>(playerIndex)).isHolding = isHolding;
}

void EffectHands::Update(const SystemMenu::InputManager& input)
{
    for (int playerIndex = 0; playerIndex < SystemMenu::PlayerCount; ++playerIndex)
    {
        bool isShown = look.showsEveryPlayer || playerIndex == 0;
        UpdateHand(playerIndex, isShown ? input.Player(playerIndex) : std::nullopt);
    }
}

void EffectHands::UpdateHand(int playerIndex, const std::optional<SystemMenu::PlayerInput>& playerInput)
{
    Hand& hand = hands[static_cast<std::size_t>(playerIndex)];
    hand.shadow.reset();
    hand.pointer.reset();
    if (!playerInput || !playerInput->state.pointerIsValid)
        return;

    Formats::Vector2 canvasPoint = display.PointerToCanvas(playerInput->state.pointer);
    Formats::Vector2 point = {canvasPoint.x / look.boxToCanvasScale.x, canvasPoint.y / look.boxToCanvasScale.y};
    if (hand.isHolding)
        point.y += look.holdingLift;

    float angle = playerInput->state.rollDegrees * RadiansPerDegree;
    if (hand.isHolding && !look.closedHandTurns)
        angle = 0;
    Formats::Vector2 shadowPoint = {point.x + look.shadowOffset.x, point.y + look.shadowOffset.y};
    hand.shadow = MakeEffect(ShadowName(hand.isHolding), shadowPoint, angle);
    hand.pointer = MakeEffect(HandName(hand.isHolding, playerIndex), point, angle);
}

std::unique_ptr<Effects::Effect> EffectHands::MakeEffect(const std::string& name, Formats::Vector2 position, float angle)
{
    auto effect = std::make_unique<Effects::Effect>(resources, name, nextSeed);
    ++nextSeed;
    effect->SetTranslation({position.x, position.y, 0});
    effect->SetRotationOffset({0, 0, angle});
    effect->Update();
    return effect;
}

// From the last player to the first, each shadow under its hand, in the order the Forecast makes them.
void EffectHands::Draw()
{
    for (auto hand = hands.rbegin(); hand != hands.rend(); ++hand)
    {
        if (hand->shadow)
            Effects::DrawEffect(*hand->shadow, camera);
        if (hand->pointer)
            Effects::DrawEffect(*hand->pointer, camera);
    }
}

int EffectHands::ShownHandCount() const
{
    int count = 0;
    for (const Hand& hand : hands)
    {
        if (hand.pointer)
            ++count;
    }
    return count;
}

}
