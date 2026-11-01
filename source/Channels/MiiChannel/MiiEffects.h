#pragma once

#include "Channels/MiiChannel/MiiFigures.h"
#include "Channels/MiiChannel/MiiPlaza.h"

#include "Effects/Effect.h"
#include "Effects/EffectResources.h"

#include "SystemMenu/Display.h"

#include "Formats/Bytes.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace Channels
{

// The effects of the Mii Channel (EffectManager.cpp of the original, CHN-94): the effects of the Miis among the 3D models (the balloons
// of a talk, dust, the sneeze, the Zzz of a nap, the sparkles of the editor) and the favorite mark over the layouts.
class MiiEffects
{
public:
    // The names of effect_nigaoe.breff (the table 0x80233EB0).
    static constexpr std::string_view TalkBalloons = "Nc_talk01";
    static constexpr std::string_view RunningDust = "Nc_run_smoke01";
    static constexpr std::string_view SlipDust = "Nc_bodyslip01";
    static constexpr std::string_view SneezeSplash = "Nc_sneeze01";
    static constexpr std::string_view SleepingZzz = "Nc_zzz01";
    static constexpr std::string_view ChangedPartSparkles = "Nc_change_parts01";
    static constexpr std::string_view FavoriteSparkles = "Nc_favor_pants01";
    static constexpr std::string_view FavoriteSparklesOfAWoman = "Nc_favor_pants01fm";
    static constexpr std::string_view FavoriteMarkSparkles = "Nc_favor_on01";

    // The display must outlive the effects.
    MiiEffects(SystemMenu::Display& display, Formats::ByteView effectFile, Formats::ByteView textureFile);

    // An effect among the Miis, placed, turned and scaled by the matrix. While the original's 20 places are busy nothing is made.
    void Start(std::string_view name, const Graphics::Matrix4& matrix);

    // An effect that follows a Mii until it fades out: the Zzz of a nap (0x800298A0).
    void StartFollowing(std::string_view name, std::size_t miiSlot, const Graphics::Matrix4& matrix);
    void Follow(std::size_t miiSlot, const Graphics::Matrix4& matrix);

    // The following effect fades out (17 of 255 a step, 0x80029E70) and ends.
    void FadeOut(std::size_t miiSlot);
    std::vector<std::size_t> FollowedMiis() const;

    // An effect over the layouts at a place of the picture (group 0 of the original): the favorite mark of the profile.
    void StartFlat(std::string_view name, Formats::Vector2 canvasPoint);

    // One step of every effect; the effects that ended are gone.
    void Update();

    // Among the 3D models, with their camera and their depth.
    void DrawAmongModels(const Graphics::Matrix4& view, const Graphics::Matrix4& projection) const;

    // Over the layouts.
    void DrawFlat() const;

    // Every effect ends at once, when the scene changes.
    void Clear();

    // The effects that play now among the models, for the tests.
    std::size_t CountAmongModels(std::string_view name) const;

private:
    struct PlayingEffect
    {
        std::string_view name;
        std::unique_ptr<Effects::Effect> effect;
        std::optional<std::size_t> followedMii;
        bool isFading = false;
        int alpha = 0;
    };

    std::unique_ptr<Effects::Effect> MakeEffect(std::string_view name);

    Effects::EffectResources resources;
    std::vector<PlayingEffect> amongModels;
    std::vector<PlayingEffect> flat;
    std::uint16_t nextSeed = 1;
};

// The effects of the moments of the Miis of a plaza or a parade, and the Zzz of the Miis that nap.
void ShowPlazaMoments(MiiEffects& effects, MiiPlaza& plaza, const MiiFigures& figures);

}
