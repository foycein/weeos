#include "Channels/WaitingMark.h"

#include "Channels/ChannelError.h"
#include "Channels/ChannelFiles.h"

#include "Formats/TextureDecoder.h"
#include "Formats/TplTexture.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace Channels
{

namespace
{

constexpr int CopyCount = 6;
constexpr int StepsPerCopy = 12;

// A copy that went out fades over six copies' time: 72 steps.
constexpr float StepsOfFading = static_cast<float>(CopyCount * StepsPerCopy);

constexpr float FullAlpha = 255;
constexpr float QuarterTurn = std::numbers::pi_v<float> / 2;

// A lit copy of the News grows by up to 8 pixels: 8 x alpha x alpha / (255 x 255).
constexpr float LargestGrowth = 8;

// The six copies stand around the middle of the screen: the first two and a half distances left of it.
constexpr float CopiesLeftOfTheMiddle = 2.5f;

// The screen of the channels is 456 pixels high; the origin of the canvas is its middle.
constexpr float HalfScreenHeight = 228;
constexpr int OpaqueAlpha = 255;

}

WaitingMark::WaitingMark(Graphics::GraphicsDevice& graphicsDevice, const Formats::U8Archive& resources, const WaitingMarkLook& markLook)
    : device(graphicsDevice)
    , look(markLook)
{
    std::vector<Formats::TplTexture> textures = Formats::ReadTplTextures(ReadPackedFile(resources, look.texturePath));
    if (look.textureIndex >= textures.size())
        throw ChannelError(std::format("{} has no picture {}", look.texturePath, look.textureIndex));

    Formats::RgbaImage picture = Formats::DecodeTexture(textures[look.textureIndex]);
    Graphics::TextureDescription description;
    description.width = picture.width;
    description.height = picture.height;
    description.pixels = picture.pixels;
    texture = device.CreateTexture(description);
    width = static_cast<float>(picture.width);
    height = static_cast<float>(picture.height);
}

WaitingMark::~WaitingMark()
{
    device.DestroyTexture(texture);
}

void WaitingMark::Update()
{
    ++step;
    if (step > look.lastStep)
        step = 0;
}

int WaitingMark::Step() const
{
    return step;
}

// The lit copy comes up along a quarter of a sine in 12 steps; the copies before it fade, the nearest one the least.
int WaitingMark::AlphaOf(int copy) const
{
    int litCopy = step / StepsPerCopy;
    int stepOfCopy = step % StepsPerCopy;
    if (copy == litCopy)
        return static_cast<int>(FullAlpha * std::sin(QuarterTurn * static_cast<float>(stepOfCopy + 1) / StepsPerCopy));

    if (look.fadesAlongSine)
    {
        int copiesSince = litCopy - copy - 1;
        if (copiesSince < 0)
            copiesSince += CopyCount;

        float stepsSince = static_cast<float>(stepOfCopy + copiesSince * StepsPerCopy);
        return static_cast<int>(FullAlpha * (1 - std::sin(QuarterTurn * stepsSince / StepsOfFading)));
    }

    int copiesSince = litCopy - copy;
    if (copiesSince < 0)
        copiesSince += CopyCount;

    float stepsLeft = std::max(StepsOfFading - static_cast<float>(stepOfCopy + copiesSince * StepsPerCopy), 0.0f);
    return static_cast<int>(FullAlpha * stepsLeft / StepsOfFading);
}

void WaitingMark::Draw(float screenWidth)
{
    float halfWidth = screenWidth / 2;
    float firstMiddle = halfWidth - CopiesLeftOfTheMiddle * look.distance;

    for (int copy = 0; copy < CopyCount; ++copy)
    {
        int alpha = AlphaOf(copy);
        float growth = 0;
        if (look.growsWhenLit)
            growth = LargestGrowth * static_cast<float>(alpha * alpha) / (FullAlpha * FullAlpha);

        float copyWidth = look.scale * width + growth;
        float copyHeight = look.scale * height + growth;
        float middleX = firstMiddle + look.distance * static_cast<float>(copy);
        float left = middleX - copyWidth / 2 - halfWidth;
        float top = HalfScreenHeight - (look.middleY - look.scale * height / 2 - growth);
        float right = left + copyWidth;
        float bottom = top - copyHeight;

        auto copyAlpha = static_cast<std::uint8_t>(std::clamp(alpha, 0, OpaqueAlpha));
        Formats::Rgba8Color color = {OpaqueAlpha, OpaqueAlpha, OpaqueAlpha, copyAlpha};
        Graphics::Quad quad{};
        quad.positions = {Formats::Vector2{left, top}, {right, top}, {right, bottom}, {left, bottom}};
        quad.colors = {color, color, color, color};
        quad.textureCoordinates[0] = {Formats::Vector2{0, 0}, {1, 0}, {1, 1}, {0, 1}};

        Graphics::Material material;
        material.textures.push_back({texture, Graphics::TextureMatrix()});
        device.DrawQuad(quad, material);
    }
}

}
