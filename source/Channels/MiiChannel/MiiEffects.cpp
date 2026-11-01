#include "Channels/MiiChannel/MiiEffects.h"

#include "Effects/ParticleDrawing.h"

#include <algorithm>

namespace Channels
{

namespace
{

// The original's places for effects among the models (EffectManager +0x94, 20 of 0x18 bytes) and over the layouts (+0x44, 5).
constexpr std::size_t MostEffectsAmongModels = 20;
constexpr std::size_t MostFlatEffects = 5;

// A following effect fades out by 17 of 255 a step (0x80029E70).
constexpr int FullAlpha = 255;
constexpr int FadePerStep = 17;

// The channel shrinks the balloons of a talk and the splash of a sneeze to 0.8 (0x803BE364).
constexpr float TalkAndSneezeScale = 0.8f;

// The flat effects use the 2D box of the hand: 810 by 448 units in 16:9 (0x80028608).
constexpr float FlatBoxHalfWidth = 304.0f * 4 / 3;
constexpr float FlatBoxHalfHeight = 224;

// The sneeze and the Zzz stand in a frame of the Mii that turns with it and whose origin lies this far below the head (EFF-13,
// Assumed): the head of a Mii of the default size stands 94.5 units above its feet (MiiBodyTest), and the effects' own translations
// (110 and 60 units up) are measured from the feet.
constexpr float HeadHeightAboveTheFeet = 94.5f;

Graphics::Matrix4 FrameUnderTheHead(const PlazaMii& plazaMii, const MiiFigures& figures)
{
    Formats::Vector3 head = figures.HeadPositionOf(plazaMii);
    return Graphics::MakeTranslation(head) * Graphics::MakeRotationAroundY(plazaMii.facingDegrees) *
        Graphics::MakeTranslation({0, -HeadHeightAboveTheFeet, 0});
}

Graphics::Matrix4 FeetFrame(const PlazaMii& plazaMii)
{
    return Graphics::MakeTranslation({plazaMii.position.x, 0, plazaMii.position.y}) * Graphics::MakeRotationAroundY(plazaMii.facingDegrees);
}

Graphics::Matrix4 Shrunk(const Graphics::Matrix4& matrix)
{
    return matrix * Graphics::MakeScale({TalkAndSneezeScale, TalkAndSneezeScale, TalkAndSneezeScale});
}

void ShowMoment(MiiEffects& effects, const PlazaMomentOfMii& moment, const PlazaMii& plazaMii, const MiiFigures& figures)
{
    switch (moment.moment)
    {
    case PlazaMoment::Talks:
        effects.Start(MiiEffects::TalkBalloons, Shrunk(Graphics::MakeTranslation(figures.HeadPositionOf(plazaMii))));
        break;
    case PlazaMoment::RaisesDust:
        effects.Start(MiiEffects::RunningDust, FeetFrame(plazaMii));
        break;
    case PlazaMoment::Slips:
        effects.Start(MiiEffects::SlipDust, Graphics::MakeTranslation({plazaMii.position.x, 0, plazaMii.position.y}));
        break;
    case PlazaMoment::Sneezes:
        effects.Start(MiiEffects::SneezeSplash, Shrunk(FrameUnderTheHead(plazaMii, figures)));
        break;
    case PlazaMoment::FallsAsleep:
        effects.StartFollowing(MiiEffects::SleepingZzz, moment.slot, FrameUnderTheHead(plazaMii, figures));
        break;
    }
}

}

MiiEffects::MiiEffects(SystemMenu::Display& display, Formats::ByteView effectFile, Formats::ByteView textureFile)
    : resources(display.Device())
{
    resources.AddFiles(effectFile, textureFile);
}

std::unique_ptr<Effects::Effect> MiiEffects::MakeEffect(std::string_view name)
{
    auto effect = std::make_unique<Effects::Effect>(resources, name, nextSeed);
    ++nextSeed;
    return effect;
}

void MiiEffects::Start(std::string_view name, const Graphics::Matrix4& matrix)
{
    if (amongModels.size() >= MostEffectsAmongModels)
        return;

    PlayingEffect playing{name, MakeEffect(name)};
    playing.effect->SetMatrix(matrix);
    amongModels.push_back(std::move(playing));
}

void MiiEffects::StartFollowing(std::string_view name, std::size_t miiSlot, const Graphics::Matrix4& matrix)
{
    if (amongModels.size() >= MostEffectsAmongModels)
        return;

    PlayingEffect playing{name, MakeEffect(name), miiSlot};
    playing.effect->SetMatrix(matrix);
    amongModels.push_back(std::move(playing));
}

void MiiEffects::Follow(std::size_t miiSlot, const Graphics::Matrix4& matrix)
{
    for (PlayingEffect& playing : amongModels)
    {
        if (playing.followedMii == miiSlot && !playing.isFading)
            playing.effect->SetMatrix(matrix);
    }
}

void MiiEffects::FadeOut(std::size_t miiSlot)
{
    for (PlayingEffect& playing : amongModels)
    {
        if (playing.followedMii != miiSlot || playing.isFading)
            continue;
        playing.isFading = true;
        playing.alpha = FullAlpha;
    }
}

std::vector<std::size_t> MiiEffects::FollowedMiis() const
{
    std::vector<std::size_t> slots;
    for (const PlayingEffect& playing : amongModels)
    {
        if (playing.followedMii && !playing.isFading)
            slots.push_back(*playing.followedMii);
    }
    return slots;
}

void MiiEffects::StartFlat(std::string_view name, Formats::Vector2 canvasPoint)
{
    if (flat.size() >= MostFlatEffects)
        return;

    float scaleX = SystemMenu::WideCanvas.width / 2 / FlatBoxHalfWidth;
    float scaleY = SystemMenu::WideCanvas.height / 2 / FlatBoxHalfHeight;
    PlayingEffect playing{name, MakeEffect(name)};
    playing.effect->SetTranslation({canvasPoint.x / scaleX, canvasPoint.y / scaleY, 0});
    flat.push_back(std::move(playing));
}

void MiiEffects::Update()
{
    for (std::vector<PlayingEffect>* group : {&amongModels, &flat})
    {
        for (PlayingEffect& playing : *group)
        {
            if (playing.isFading)
            {
                playing.alpha -= FadePerStep;
                std::uint8_t alpha = static_cast<std::uint8_t>(std::max(playing.alpha, 0));
                playing.effect->SetColorMultiplier({FullAlpha, FullAlpha, FullAlpha, alpha});
            }
            playing.effect->Update();
        }
        std::erase_if(*group, [](const PlayingEffect& playing) {
            bool hasFadedOut = playing.isFading && playing.alpha <= 0;
            return hasFadedOut || playing.effect->IsFinished();
        });
    }
}

void MiiEffects::DrawAmongModels(const Graphics::Matrix4& view, const Graphics::Matrix4& projection) const
{
    Effects::EffectCamera camera;
    camera.view = view;
    camera.projection = projection;
    for (const PlayingEffect& playing : amongModels)
        Effects::DrawEffect(*playing.effect, camera);
}

void MiiEffects::DrawFlat() const
{
    Effects::EffectCamera camera;
    camera.boxToCanvasScale = {SystemMenu::WideCanvas.width / 2 / FlatBoxHalfWidth, SystemMenu::WideCanvas.height / 2 / FlatBoxHalfHeight};
    for (const PlayingEffect& playing : flat)
        Effects::DrawEffect(*playing.effect, camera);
}

void MiiEffects::Clear()
{
    amongModels.clear();
    flat.clear();
}

std::size_t MiiEffects::CountAmongModels(std::string_view name) const
{
    return static_cast<std::size_t>(std::count_if(amongModels.begin(), amongModels.end(),
        [name](const PlayingEffect& playing) { return playing.name == name; }));
}

void ShowPlazaMoments(MiiEffects& effects, MiiPlaza& plaza, const MiiFigures& figures)
{
    for (const PlazaMomentOfMii& moment : plaza.TakeMoments())
    {
        const PlazaMii* plazaMii = plaza.FindMii(moment.slot);
        if (plazaMii != nullptr)
            ShowMoment(effects, moment, *plazaMii, figures);
    }

    for (std::size_t slot : effects.FollowedMiis())
    {
        const PlazaMii* plazaMii = plaza.FindMii(slot);
        bool stillNaps = plazaMii != nullptr && plazaMii->activity == PlazaActivity::Napping;
        if (stillNaps)
            effects.Follow(slot, FrameUnderTheHead(*plazaMii, figures));
        else
            effects.FadeOut(slot);
    }
}

}
