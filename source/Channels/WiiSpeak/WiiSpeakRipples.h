#pragma once

#include "Effects/Effect.h"
#include "Effects/EffectResources.h"

#include "SystemMenu/Display.h"

#include "Formats/Bytes.h"
#include "Formats/Vectors.h"

#include <memory>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

namespace Channels
{

// The ripples on the water of the Wii Speak Channel (ef/ripple_effect of system.szs): the rings of the clock at the full hour,
// the drops that fall on the water, the ring of a photo that lands, the Miis that appear in the circle and the sparkle of a Mii that
// is put into a circle. They are drawn over the water and under the buttons, in the box of the channel's projection: 810.7 by 456
// units around the middle (4/3 of 608 by 456 in 16:9, 0x800BF6E4).
class WiiSpeakRipples
{
public:
    static constexpr std::string_view ClockRipple = "Mic_ripple01";
    static constexpr std::string_view ChatRoomDrop = "Mic_ripple_small01";
    static constexpr std::string_view MenuDrop = "Mic_ripple_small05";
    static constexpr std::string_view PhotoRipple = "Mic_photo_ripple01";
    static constexpr std::string_view MiisAppear = "Mic_appear01";
    static constexpr std::string_view MiiPutIntoACircle = "Mic_miiset01";

    // The display must outlive the ripples.
    WiiSpeakRipples(SystemMenu::Display& display, Formats::ByteView effectFile, Formats::ByteView textureFile, std::uint32_t randomSeed);

    // The ring of the clock in the last three seconds before a full hour (small) and at the full hour (large) (0x800A623C, 0x800A64A8).
    void StartClockRipple(Formats::Vector2 canvasPlace, bool isTheFullHour);

    // A drop at a random place of the water.
    void StartDrop(std::string_view name);

    // The white ring of a photo that lands on the water (0x80051968).
    void StartPhotoRipple(Formats::Vector2 canvasPlace);

    // The Miis come up in the circle: a large ring, and 25 steps later their own ripples (0x800B2988).
    void StartMiisAppearing(Formats::Vector2 canvasPlace);

    // A Mii is put into a circle of "Choose Miis" (0x80072F58).
    void StartMiiPutIntoACircle(Formats::Vector2 canvasPlace);

    void Update();
    void Draw() const;

    // Every ripple ends, when the screen changes.
    void Clear();

    // The ripples of a name that play now, for the tests.
    std::size_t Count(std::string_view name) const;

private:
    struct PlayingRipple
    {
        std::string_view name;
        std::unique_ptr<Effects::Effect> effect;
    };

    Effects::Effect& Start(std::string_view name, const Graphics::Matrix4& matrix);
    Graphics::Matrix4 OnTheWater(Formats::Vector2 canvasPlace, float scale) const;
    Formats::Vector2 BoxPointOf(Formats::Vector2 canvasPlace) const;

    Effects::EffectResources resources;
    Formats::Vector2 boxToCanvasScale;
    std::vector<PlayingRipple> ripples;
    std::optional<Formats::Vector2> appearingPlace;
    int stepsUntilTheMiisAppear = 0;
    std::mt19937 random;
    std::uint16_t nextSeed = 1;
};

}
