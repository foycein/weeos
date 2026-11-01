#include "Channels/WiiSpeak/WiiSpeakRipples.h"

#include "Effects/ParticleDrawing.h"

#include <algorithm>

namespace Channels
{

namespace
{

// The box of the projection: 608 by 456 in 4:3, 4/3 wider in 16:9 (0x8019EA78).
constexpr float BoxHalfWidth = 304.0f * 4 / 3;
constexpr float BoxHalfHeight = 228;

// The rings lie flat on the water: the program turns them a quarter about X and makes them 200 times larger (0x800A623C). The ring of
// the clock is faint (alpha 60), the one at the full hour twice as wide and faster (0x800A64A8).
constexpr float QuarterTurnDegrees = 90;
constexpr float RingScale = 200;
constexpr std::uint8_t ClockRippleAlpha = 60;
constexpr float SmallRingWidth = 30;
constexpr float LargeRingWidth = 60;
constexpr float SmallRingSpeed = 0.012f;
constexpr float LargeRingSpeed = 0.04f;
constexpr std::uint16_t SmallRingLife = 90;
constexpr std::uint16_t LargeRingLife = 60;

// The drops are a little brighter than the rings of the clock (alpha 100, 0x800A6570).
constexpr std::uint8_t DropAlpha = 100;
constexpr std::uint8_t FullIntensity = 255;

// The Miis' own ripples come 25 steps after the large ring, 2.3 times larger (0x800B2988, 0x800A6414).
constexpr int StepsBeforeTheMiisAppear = 25;
constexpr float AppearingScale = 2.3f;

}

WiiSpeakRipples::WiiSpeakRipples(SystemMenu::Display& display, Formats::ByteView effectFile, Formats::ByteView textureFile,
    std::uint32_t randomSeed)
    : resources(display.Device())
    , boxToCanvasScale{SystemMenu::WideCanvas.width / 2 / BoxHalfWidth, SystemMenu::WideCanvas.height / 2 / BoxHalfHeight}
    , random(randomSeed)
{
    resources.AddFiles(effectFile, textureFile);
}

Formats::Vector2 WiiSpeakRipples::BoxPointOf(Formats::Vector2 canvasPlace) const
{
    return {canvasPlace.x / boxToCanvasScale.x, canvasPlace.y / boxToCanvasScale.y};
}

Graphics::Matrix4 WiiSpeakRipples::OnTheWater(Formats::Vector2 canvasPlace, float scale) const
{
    Formats::Vector2 point = BoxPointOf(canvasPlace);
    return Graphics::MakeTranslation({point.x, point.y, 0}) * Graphics::MakeRotationAroundX(QuarterTurnDegrees) *
        Graphics::MakeScale({scale, scale, scale});
}

Effects::Effect& WiiSpeakRipples::Start(std::string_view name, const Graphics::Matrix4& matrix)
{
    auto effect = std::make_unique<Effects::Effect>(resources, name, nextSeed);
    ++nextSeed;
    effect->SetMatrix(matrix);
    ripples.push_back({name, std::move(effect)});
    return *ripples.back().effect;
}

void WiiSpeakRipples::StartClockRipple(Formats::Vector2 canvasPlace, bool isTheFullHour)
{
    Effects::Effect& ripple = Start(ClockRipple, OnTheWater(canvasPlace, RingScale));
    float width = isTheFullHour ? LargeRingWidth : SmallRingWidth;
    ripple.SetColorMultiplier({FullIntensity, FullIntensity, FullIntensity, ClockRippleAlpha});
    ripple.SetSizeMultiplier({width, width});
    ripple.SetRadiationSpeed(isTheFullHour ? LargeRingSpeed : SmallRingSpeed);
    ripple.SetParticleLife(isTheFullHour ? LargeRingLife : SmallRingLife);
    ripple.SetInitialVelocityRandomPercent(0);
}

void WiiSpeakRipples::StartDrop(std::string_view name)
{
    std::uniform_real_distribution<float> across(-BoxHalfWidth, BoxHalfWidth);
    std::uniform_real_distribution<float> upward(-BoxHalfHeight, BoxHalfHeight);
    float x = across(random);
    float y = upward(random);
    Effects::Effect& drop = Start(name, Graphics::MakeTranslation({x, y, 0}));
    drop.SetColorMultiplier({FullIntensity, FullIntensity, FullIntensity, DropAlpha});
}

void WiiSpeakRipples::StartPhotoRipple(Formats::Vector2 canvasPlace)
{
    Formats::Vector2 point = BoxPointOf(canvasPlace);
    Start(PhotoRipple, Graphics::MakeTranslation({point.x, point.y, 0}));
}

void WiiSpeakRipples::StartMiisAppearing(Formats::Vector2 canvasPlace)
{
    StartClockRipple(canvasPlace, true);
    appearingPlace = canvasPlace;
    stepsUntilTheMiisAppear = StepsBeforeTheMiisAppear;
}

void WiiSpeakRipples::StartMiiPutIntoACircle(Formats::Vector2 canvasPlace)
{
    Formats::Vector2 point = BoxPointOf(canvasPlace);
    Start(MiiPutIntoACircle, Graphics::MakeTranslation({point.x, point.y, 0}));
}

void WiiSpeakRipples::Update()
{
    if (appearingPlace)
    {
        --stepsUntilTheMiisAppear;
        if (stepsUntilTheMiisAppear <= 0)
        {
            Start(MiisAppear, OnTheWater(*appearingPlace, AppearingScale));
            appearingPlace.reset();
        }
    }

    for (PlayingRipple& ripple : ripples)
        ripple.effect->Update();
    std::erase_if(ripples, [](const PlayingRipple& ripple) { return ripple.effect->IsFinished(); });
}

void WiiSpeakRipples::Draw() const
{
    Effects::EffectCamera camera;
    camera.boxToCanvasScale = boxToCanvasScale;
    for (const PlayingRipple& ripple : ripples)
        Effects::DrawEffect(*ripple.effect, camera);
}

void WiiSpeakRipples::Clear()
{
    ripples.clear();
    appearingPlace.reset();
}

std::size_t WiiSpeakRipples::Count(std::string_view name) const
{
    return static_cast<std::size_t>(std::count_if(ripples.begin(), ripples.end(), [name](const PlayingRipple& ripple) { return ripple.name == name; }));
}

}
