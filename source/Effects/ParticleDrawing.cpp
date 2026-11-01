#include "Effects/ParticleDrawing.h"

#include "Effects/VectorMath.h"

#include <array>
#include <cmath>

namespace Effects
{

using Formats::EffectDrawSetting;
using Formats::EmitterResource;
using Formats::ParticleColorSource;
using Formats::Rgba8Color;

namespace
{

constexpr float SmallestSize = 1.19e-7f;
constexpr float StretchPerSpeed = 0.5f;

// The chip's blend factors.
constexpr std::uint8_t NoBlend = 0;
constexpr std::uint8_t Blend = 1;
constexpr std::uint8_t FactorZero = 0;
constexpr std::uint8_t FactorOne = 1;
constexpr std::uint8_t FactorSourceAlpha = 4;
constexpr std::uint8_t FactorInverseSourceAlpha = 5;

constexpr std::uint8_t AlwaysPasses = 7;
constexpr std::uint8_t AmbientLighting = 1;
constexpr std::uint8_t NoStageTexture = 0xFF;
constexpr std::size_t StageTextureCount = 2;
constexpr std::size_t IndirectTextureSlot = 2;

// The bytes of a stage's color or alpha operation, in their order in the file.
constexpr std::size_t OperationKind = 0;
constexpr std::size_t OperationBias = 1;
constexpr std::size_t OperationScale = 2;
constexpr std::size_t OperationClamp = 3;
constexpr std::size_t OperationOutput = 4;

constexpr std::uint8_t WrapBitsMask = 0xF;
constexpr std::size_t BitsPerTextureWrap = 4;
constexpr std::uint8_t ReverseBitsMask = 0x3;
constexpr std::size_t BitsPerTextureReverse = 2;
constexpr std::uint8_t WrapMask = 0x3;
constexpr int VerticalWrapShift = 2;
constexpr std::uint8_t MirrorWrap = 2;
constexpr std::uint8_t ReversesHorizontally = 0x1;
constexpr std::uint8_t ReversesVertically = 0x2;

// A mirrored wrap shows the picture twice over the texture coordinates of one picture.
constexpr float MirrorRepeat = 2;
constexpr std::size_t TextureTranslationColumn = 2;

constexpr Rgba8Color OpaqueWhite = {255, 255, 255, 255};

// A quad among the models is drawn as two triangles of three corners.
constexpr std::size_t TwoTrianglesCornerCount = 6;

// The chip multiplies two 8-bit colors as (a * b + 128) / 256.
constexpr int ProductRounding = 0x80;
constexpr int ProductShift = 8;

// One corner of a particle's picture in view space.
struct ViewCorner
{
    Vector3 position;
    Vector2 textureCoordinate;
};

// Left top, right top, right bottom, left bottom.
using ViewQuad = std::array<ViewCorner, Graphics::QuadCornerCount>;

// What every particle of one emitter needs for its picture.
struct EmitterView
{
    const Effect& effect;
    const EmitterResource& resource;
    const Graphics::Matrix4& cameraView;

    // The camera's view of the particles' space: its lengths along x and y and its roll.
    float scaleX = 1;
    float scaleY = 1;
    float rollCosine = 1;
    float rollSine = 0;

    // The emitter in the world.
    Vector3 origin = {0, 0, 0};
    Vector3 upAxis = {0, 1, 0};
};

EmitterView MakeEmitterView(const Effect& effect, const Emitter& emitter, const EffectCamera& camera)
{
    EmitterView view{effect, emitter.Resource(), camera.view};
    Graphics::Matrix4 particleView = camera.view * emitter.ParticleSpaceMatrix();
    view.scaleX = Length(ColumnOf(particleView, 0));
    view.scaleY = Length(ColumnOf(particleView, 1));

    float upX = particleView.rows[0][1];
    float upY = particleView.rows[1][1];
    float upLength = std::sqrt(upX * upX + upY * upY);
    if (upLength > 0)
    {
        view.rollCosine = upY / upLength;
        view.rollSine = -upX / upLength;
    }

    Graphics::Matrix4 world = emitter.WorldMatrix();
    view.origin = ColumnOf(world, TranslationColumn);
    view.upAxis = NormalizeOr(ColumnOf(world, 1), {0, 1, 0});
    return view;
}

float HalfWidth(const EmitterView& view, const Particle& particle)
{
    return particle.size.x * particle.scale.x * view.effect.SizeMultiplier().x;
}

float HalfHeight(const EmitterView& view, const Particle& particle)
{
    return ParticleHeight(particle, view.resource.shape.heightRule) * view.effect.SizeMultiplier().y;
}

// A point of the particle's picture plane (x right, y up) turned by the camera's roll and scaled into view space.
Vector3 RollIntoView(const EmitterView& view, float x, float y)
{
    float viewX = view.scaleX * view.rollCosine * x - view.scaleY * view.rollSine * y;
    float viewY = view.scaleX * view.rollSine * x + view.scaleY * view.rollCosine * y;
    return {viewX, viewY, 0};
}

Vector2 Turn(float x, float y, float angle)
{
    float cosine = std::cos(angle);
    float sine = std::sin(angle);
    return {cosine * x - sine * y, sine * x + cosine * y};
}

// The texture coordinates run from 0 at the left and the top to 1 at the right and the bottom of the picture.
ViewCorner CornerOf(Vector3 position, float pictureX, float pictureY)
{
    return {position, {pictureX, pictureY}};
}

// A picture that faces the viewer, turned by the particle's z rotation.
ViewQuad FacingQuad(const EmitterView& view, const Particle& particle, float width, float height)
{
    Vector3 center = Graphics::TransformPoint(view.cameraView, particle.position);
    float angle = particle.rotation.z + particle.rotationOffset.z + view.effect.RotationOffset().z;

    Vector2 pivot = view.resource.shape.pivot;
    Vector2 pivotOffset = Turn(pivot.x - width * pivot.x, height * pivot.y - pivot.y, angle);
    center = Add(center, RollIntoView(view, pivotOffset.x, pivotOffset.y));

    auto corner = [&](float x, float y, float pictureX, float pictureY) {
        Vector2 turned = Turn(x, y, angle);
        return CornerOf(Add(center, RollIntoView(view, turned.x, turned.y)), pictureX, pictureY);
    };
    return {
        corner(-width, height, 0, 0),
        corner(width, height, 1, 0),
        corner(width, -height, 1, 1),
        corner(-width, -height, 0, 1),
    };
}

Vector3 ParticleDirection(const EmitterView& view, const Particle& particle)
{
    Vector3 direction = Subtract(particle.position, particle.previousPosition);
    if (view.resource.shape.direction == Formats::ParticleDirection::FromTheEmitter)
        direction = Subtract(particle.position, view.origin);
    return NormalizeOr(direction, view.upAxis);
}

float Stretch(const EmitterView& view, const Particle& particle, float height)
{
    if (view.resource.shape.stretchesWithSpeed == 0)
        return 1;
    return 1 + StretchPerSpeed * Length(Subtract(particle.position, particle.previousPosition)) / height;
}

// A picture that faces the viewer with its top along the particle's way across the picture.
ViewQuad AlongTheDirectionQuad(const EmitterView& view, const Particle& particle, float width, float height)
{
    Vector3 position = Graphics::TransformPoint(view.cameraView, particle.position);
    Vector3 direction = Graphics::TransformDirection(view.cameraView, ParticleDirection(view, particle));

    float cosine = 1;
    float sine = 0;
    if (direction.y != 0)
    {
        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        cosine = direction.y / length;
        sine = -direction.x / length;
    }
    Vector3 across = {view.scaleX * cosine, view.scaleX * sine, 0};
    Vector3 along = {-view.scaleY * sine, view.scaleY * cosine, 0};

    float stretch = Stretch(view, particle, height);
    Vector2 pivot = view.resource.shape.pivot;
    Vector3 center = Add(position, Multiply(across, pivot.x - width * pivot.x));
    center = Subtract(center, Multiply(along, height * (stretch + pivot.y - 1) - pivot.y));

    Vector3 halfAcross = Multiply(across, width);
    Vector3 halfAlong = Multiply(along, stretch * height);
    Vector3 top = Add(center, halfAlong);
    Vector3 bottom = Subtract(center, halfAlong);
    return {
        CornerOf(Subtract(top, halfAcross), 0, 0),
        CornerOf(Add(top, halfAcross), 1, 0),
        CornerOf(Add(bottom, halfAcross), 1, 1),
        CornerOf(Subtract(bottom, halfAcross), 0, 1),
    };
}

// The turn of a directional picture about the one axis the shape names: 0 is X, 1 is Y, anything else Z.
Graphics::Matrix4 TurnAboutTheShapeAxis(std::uint8_t turnAxis, Vector3 rotation)
{
    constexpr std::uint8_t TurnsAboutX = 0;
    constexpr std::uint8_t TurnsAboutY = 1;

    if (turnAxis == TurnsAboutX)
        return MakeRotationXyz({rotation.x, 0, 0});
    if (turnAxis == TurnsAboutY)
        return MakeRotationXyz({0, rotation.y, 0});
    return MakeRotationXyz({0, 0, rotation.z});
}

// A picture lying in the world: its height along the particle's direction, its width across it in the plane of the emitter's form
// (EFF-06, Assumed), turned about one of its own axes by the particle's rotation.
ViewQuad DirectionalQuad(const EmitterView& view, const Particle& particle, float width, float height)
{
    Vector3 direction = ParticleDirection(view, particle);
    Vector3 side = NormalizeOr(Cross(view.upAxis, direction), {0, 0, 1});
    Vector3 upward = Cross(direction, side);

    Vector3 rotation = Add(Add(particle.rotation, particle.rotationOffset), view.effect.RotationOffset());
    Graphics::Matrix4 turn = TurnAboutTheShapeAxis(view.resource.shape.axis, rotation);

    float stretch = Stretch(view, particle, height);
    Vector2 pivot = view.resource.shape.pivot;
    Vector3 pivotShift = Graphics::TransformDirection(turn, {width * pivot.x, height * (pivot.y + stretch - 1), 0});
    Vector3 shift = Subtract({pivot.x, pivot.y, 0}, pivotShift);

    auto corner = [&](float x, float y, float pictureX, float pictureY) {
        Vector3 local = Add(Graphics::TransformDirection(turn, {x * width, y * stretch * height, 0}), shift);
        Vector3 inTheWorld = Add(Add(Multiply(side, local.x), Multiply(direction, local.y)), Multiply(upward, local.z));
        Vector3 world = Add(particle.position, inTheWorld);
        return CornerOf(Graphics::TransformPoint(view.cameraView, world), pictureX, pictureY);
    };
    return {
        corner(-1, 1, 0, 0),
        corner(1, 1, 1, 0),
        corner(1, -1, 1, 1),
        corner(-1, -1, 0, 1),
    };
}

// The two edges of a stripe at one particle, and how far along the stripe's texture it lies.
struct StripePoint
{
    Vector3 positive;
    Vector3 negative;
    float along;
};

// The weights of three neighbouring points of a quadratic B-spline at a fraction of the way through the middle segment.
struct SplineWeights
{
    float first;
    float second;
    float third;
};

SplineWeights WeightsAt(float fraction)
{
    float rest = 1 - fraction;
    return {0.5f * rest * rest, 0.5f + fraction - fraction * fraction, 0.5f * fraction * fraction};
}

Vector3 Weighted(Vector3 first, Vector3 second, Vector3 third, const SplineWeights& weights)
{
    Vector3 firstShare = Multiply(first, weights.first);
    Vector3 secondShare = Multiply(second, weights.second);
    Vector3 thirdShare = Multiply(third, weights.third);
    return Add(Add(firstShare, secondShare), thirdShare);
}

StripePoint PointOnTheSpline(const StripePoint& first, const StripePoint& second, const StripePoint& third, float fraction)
{
    SplineWeights weights = WeightsAt(fraction);
    StripePoint point;
    point.positive = Weighted(first.positive, second.positive, third.positive, weights);
    point.negative = Weighted(first.negative, second.negative, third.negative, weights);
    point.along = weights.first * first.along + weights.second * second.along + weights.third * third.along;
    return point;
}

// The particles in the order the stripe runs through them: the newest first, unless the draw setting asks for the oldest first.
std::vector<const Particle*> WalkOfTheStripe(const EmitterView& view, const std::vector<Particle>& particles)
{
    std::vector<const Particle*> walk;
    bool oldestFirst = (view.resource.drawSetting.flags & Formats::DrawFlag::OldestFirst) != 0;
    for (std::size_t index = 0; index < particles.size(); ++index)
        walk.push_back(oldestFirst ? &particles[index] : &particles[particles.size() - 1 - index]);
    return walk;
}

// The two edges of the stripe at one particle of the walk: across the way from its neighbours, as wide as the particle.
StripePoint EdgesAt(const EmitterView& view, const std::vector<const Particle*>& walk, std::size_t index, float along)
{
    std::size_t count = walk.size();
    const Particle& particle = *walk[index];
    const Particle& next = *walk[(index + 1) % count];
    const Particle& previous = *walk[(index + count - 1) % count];
    Vector3 toNext = NormalizeOr(Subtract(next.position, particle.position), {0, 0, 0});
    Vector3 toPrevious = NormalizeOr(Subtract(previous.position, particle.position), {0, 0, 0});
    Vector3 direction = NormalizeOr(Subtract(toNext, toPrevious), view.upAxis);
    Vector3 side = NormalizeOr(Cross(direction, view.upAxis), {1, 0, 0});
    Vector3 axis = NormalizeOr(Cross(side, direction), view.upAxis);

    float width = HalfWidth(view, particle);
    float turn = particle.rotation.y + particle.rotationOffset.y + view.effect.RotationOffset().y;
    float pivot = view.resource.shape.pivot.x;
    auto edge = [&](float sign) {
        Vector3 sideways = Multiply(side, pivot + width * std::cos(turn) * (sign - pivot));
        Vector3 upward = Multiply(axis, width * std::sin(turn) * (sign - pivot));
        return Add(particle.position, Add(sideways, upward));
    };
    return {edge(1), edge(-1), along};
}

// The quads between consecutive points of the strip, in view space.
std::vector<ViewQuad> QuadsOfTheStrip(const EmitterView& view, const std::vector<StripePoint>& strip)
{
    std::vector<ViewQuad> quads;
    for (std::size_t index = 0; index + 1 < strip.size(); ++index)
    {
        const StripePoint& start = strip[index];
        const StripePoint& end = strip[index + 1];
        quads.push_back({
            CornerOf(Graphics::TransformPoint(view.cameraView, start.positive), 1, start.along),
            CornerOf(Graphics::TransformPoint(view.cameraView, end.positive), 1, end.along),
            CornerOf(Graphics::TransformPoint(view.cameraView, end.negative), 0, end.along),
            CornerOf(Graphics::TransformPoint(view.cameraView, start.negative), 0, start.along),
        });
    }
    return quads;
}

// A ring of particles joined into a closed ribbon that bends smoothly through them (a quadratic B-spline). The texture runs along the
// ribbon from (i - 0.5) / count at the i-th particle, or once per segment.
std::vector<ViewQuad> SmoothStripeQuads(const EmitterView& view, const std::vector<Particle>& particles)
{
    constexpr std::size_t FewestParticlesOfALoop = 3;

    std::vector<const Particle*> walk = WalkOfTheStripe(view, particles);
    std::size_t count = walk.size();
    if (count < FewestParticlesOfALoop)
        return {};

    const Formats::ParticleShape& shape = view.resource.shape;
    bool texturePerSegment = (shape.stripeFlags & Formats::StripeTexturePerSegmentMask) == Formats::StripeTexturePerSegment;
    float alongStep = texturePerSegment ? 0.5f : 0.5f / static_cast<float>(count);

    std::vector<StripePoint> points;
    for (std::size_t index = 0; index < count; ++index)
        points.push_back(EdgesAt(view, walk, index, (2 * static_cast<float>(index) - 1) * alongStep));
    points.push_back({points[0].positive, points[0].negative, (2 * static_cast<float>(count) - 1) * alongStep});
    points.push_back({points[1].positive, points[1].negative, (2 * static_cast<float>(count) + 1) * alongStep});

    std::uint32_t subdivisions = shape.stripeSubdivisions == 0 ? 1u : shape.stripeSubdivisions;
    std::vector<StripePoint> strip;
    for (std::size_t index = 1; index <= count; ++index)
    {
        for (std::uint32_t piece = 0; piece < subdivisions; ++piece)
        {
            float fraction = static_cast<float>(piece) / static_cast<float>(subdivisions);
            strip.push_back(PointOnTheSpline(points[index - 1], points[index], points[index + 1], fraction));
        }
    }
    strip.push_back(PointOnTheSpline(points[count], points[count], points[count + 1], 1.0f));
    return QuadsOfTheStrip(view, strip);
}

std::uint8_t ChannelProduct(std::uint8_t first, std::uint8_t second)
{
    return static_cast<std::uint8_t>((first * second + ProductRounding) >> ProductShift);
}

Rgba8Color Product(Rgba8Color first, Rgba8Color second)
{
    Rgba8Color product;
    product.red = ChannelProduct(first.red, second.red);
    product.green = ChannelProduct(first.green, second.green);
    product.blue = ChannelProduct(first.blue, second.blue);
    product.alpha = ChannelProduct(first.alpha, second.alpha);
    return product;
}

std::array<Rgba8Color, Formats::ParticleColorCount> ShownColors(const EmitterView& view, const Particle& particle)
{
    std::array<Rgba8Color, Formats::ParticleColorCount> colors = particle.colors;
    const EffectDrawSetting& drawSetting = view.resource.drawSetting;
    std::optional<Rgba8Color> multiplier = view.effect.ColorMultiplier();
    for (Rgba8Color& color : colors)
    {
        if (drawSetting.lightingType == AmbientLighting)
            color = Product(color, drawSetting.ambientColor);
        if (multiplier)
            color = Product(color, *multiplier);
    }
    return colors;
}

std::optional<Rgba8Color> ColorOfSource(const std::array<Rgba8Color, Formats::ParticleColorCount>& colors, ParticleColorSource source)
{
    switch (source)
    {
    case ParticleColorSource::Color1Primary: return colors[0];
    case ParticleColorSource::Color1Secondary: return colors[1];
    case ParticleColorSource::Color2Primary: return colors[2];
    case ParticleColorSource::Color2Secondary: return colors[3];
    case ParticleColorSource::Color1PrimaryTimesSecondary: return Product(colors[0], colors[1]);
    case ParticleColorSource::Color2PrimaryTimesSecondary: return Product(colors[2], colors[3]);
    default: return std::nullopt;
    }
}

void FillColors(Graphics::Material& material, const EffectDrawSetting& drawSetting,
    const std::array<Rgba8Color, Formats::ParticleColorCount>& colors)
{
    for (std::size_t number = 0; number < Graphics::ColorRegisterCount; ++number)
    {
        Graphics::RegisterColor& target = material.colorRegisters[number];
        if (std::optional<Rgba8Color> color = ColorOfSource(colors, drawSetting.registerColors[number]))
        {
            target.red = color->red;
            target.green = color->green;
            target.blue = color->blue;
        }
        if (std::optional<Rgba8Color> alpha = ColorOfSource(colors, drawSetting.registerAlphas[number]))
            target.alpha = alpha->alpha;
    }

    for (std::size_t number = 0; number < Graphics::ConstantColorCount; ++number)
    {
        Rgba8Color& target = material.constantColors[number];
        if (std::optional<Rgba8Color> color = ColorOfSource(colors, drawSetting.constantColors[number]))
        {
            target.red = color->red;
            target.green = color->green;
            target.blue = color->blue;
        }
        if (std::optional<Rgba8Color> alpha = ColorOfSource(colors, drawSetting.constantAlphas[number]))
            target.alpha = alpha->alpha;
    }
}

Graphics::TevStage MakeStage(const Formats::EffectStage& stage, std::size_t stageNumber,
    const std::array<std::uint8_t, StageTextureCount>& materialSlots, std::uint8_t indirectStageMask)
{
    Graphics::TevStage converted;
    if (stage.textureNumber < materialSlots.size())
        converted.textureIndex = materialSlots[stage.textureNumber];
    converted.constantColor = static_cast<Graphics::TevConstant>(stage.constantColorSelection);
    converted.constantAlpha = static_cast<Graphics::TevConstant>(stage.constantAlphaSelection);

    converted.color.inputA = static_cast<Graphics::TevColorInput>(stage.colorInputs[0]);
    converted.color.inputB = static_cast<Graphics::TevColorInput>(stage.colorInputs[1]);
    converted.color.inputC = static_cast<Graphics::TevColorInput>(stage.colorInputs[2]);
    converted.color.inputD = static_cast<Graphics::TevColorInput>(stage.colorInputs[3]);
    converted.color.operation = static_cast<Graphics::TevOperation>(stage.colorOperation[OperationKind]);
    converted.color.bias = static_cast<Graphics::TevBias>(stage.colorOperation[OperationBias]);
    converted.color.scale = static_cast<Graphics::TevScale>(stage.colorOperation[OperationScale]);
    converted.color.clamp = stage.colorOperation[OperationClamp] != 0;
    converted.color.output = static_cast<Graphics::TevRegister>(stage.colorOperation[OperationOutput]);

    converted.alpha.inputA = static_cast<Graphics::TevAlphaInput>(stage.alphaInputs[0]);
    converted.alpha.inputB = static_cast<Graphics::TevAlphaInput>(stage.alphaInputs[1]);
    converted.alpha.inputC = static_cast<Graphics::TevAlphaInput>(stage.alphaInputs[2]);
    converted.alpha.inputD = static_cast<Graphics::TevAlphaInput>(stage.alphaInputs[3]);
    converted.alpha.operation = static_cast<Graphics::TevOperation>(stage.alphaOperation[OperationKind]);
    converted.alpha.bias = static_cast<Graphics::TevBias>(stage.alphaOperation[OperationBias]);
    converted.alpha.scale = static_cast<Graphics::TevScale>(stage.alphaOperation[OperationScale]);
    converted.alpha.clamp = stage.alphaOperation[OperationClamp] != 0;
    converted.alpha.output = static_cast<Graphics::TevRegister>(stage.alphaOperation[OperationOutput]);

    converted.isMovedByIndirectTexture = ((indirectStageMask >> stageNumber) & 1u) != 0;
    return converted;
}

std::uint8_t WrapBitsOf(const Particle& particle, std::size_t texture)
{
    return static_cast<std::uint8_t>((particle.textureWraps >> (texture * BitsPerTextureWrap)) & WrapBitsMask);
}

std::uint8_t ReverseBitsOf(const Particle& particle, std::size_t texture)
{
    return static_cast<std::uint8_t>((particle.textureReverses >> (texture * BitsPerTextureReverse)) & ReverseBitsMask);
}

// Adds a texture of the particle to the material; its number in the material, or none when the particle has no such picture.
std::optional<std::uint8_t> AddTexture(Graphics::Material& material, EffectResources& resources, const Particle& particle,
    std::size_t texture)
{
    std::string_view name = particle.textureNames[texture];
    if (name.empty())
        return std::nullopt;

    std::uint8_t wrapBits = WrapBitsOf(particle, texture);
    Graphics::TextureHandle handle = resources.Texture(name, wrapBits);
    if (!handle.IsValid())
        return std::nullopt;

    Graphics::TextureMatrix matrix = ParticleTextureMatrix(particle.textureTransforms[texture], wrapBits, ReverseBitsOf(particle, texture));
    material.textures.push_back({handle, matrix});
    return static_cast<std::uint8_t>(material.textures.size() - 1);
}

// Adds the indirect texture that moves the stages of the mask; the mask of the stages it moves, 0 when there is none.
std::uint8_t AddIndirectTexture(Graphics::Material& material, EffectResources& resources, const Particle& particle,
    const EffectDrawSetting& drawSetting)
{
    if (drawSetting.indirectStageMask == 0)
        return 0;

    std::optional<std::uint8_t> slot = AddTexture(material, resources, particle, IndirectTextureSlot);
    if (!slot)
        return 0;

    float scale = std::ldexp(1.0f, drawSetting.indirectScaleExponent);
    Graphics::IndirectTexture indirect;
    indirect.textureIndex = *slot;
    for (std::size_t row = 0; row < indirect.matrix.size(); ++row)
    {
        for (std::size_t column = 0; column < indirect.matrix[row].size(); ++column)
            indirect.matrix[row][column] = drawSetting.indirectMatrix[row][column] * scale;
    }
    material.indirectTexture = indirect;
    return drawSetting.indirectStageMask;
}

std::optional<Graphics::AlphaTest> AlphaTestOf(const EffectDrawSetting& drawSetting, const Particle& particle)
{
    bool testsAlpha = drawSetting.firstAlphaComparison != AlwaysPasses || drawSetting.secondAlphaComparison != AlwaysPasses;
    if (!testsAlpha)
        return std::nullopt;

    Graphics::AlphaTest test;
    test.firstComparison = static_cast<Graphics::AlphaComparison>(drawSetting.firstAlphaComparison);
    test.firstReference = particle.firstAlphaReference;
    test.secondComparison = static_cast<Graphics::AlphaComparison>(drawSetting.secondAlphaComparison);
    test.secondReference = particle.secondAlphaReference;
    test.join = static_cast<Graphics::AlphaJoin>(drawSetting.alphaComparisonJoin);
    return test;
}

Graphics::Material ParticleMaterial(const EmitterView& view, const Particle& particle)
{
    const EffectDrawSetting& drawSetting = view.resource.drawSetting;
    EffectResources& resources = view.effect.Resources();

    Graphics::Material material;
    std::array<std::uint8_t, StageTextureCount> materialSlots = {NoStageTexture, NoStageTexture};
    for (std::size_t texture = 0; texture < materialSlots.size(); ++texture)
    {
        if (std::optional<std::uint8_t> slot = AddTexture(material, resources, particle, texture))
            materialSlots[texture] = *slot;
    }

    std::uint8_t indirectStageMask = AddIndirectTexture(material, resources, particle, drawSetting);
    for (std::size_t stage = 0; stage < drawSetting.stages.size(); ++stage)
        material.stages.push_back(MakeStage(drawSetting.stages[stage], stage, materialSlots, indirectStageMask));

    FillColors(material, drawSetting, ShownColors(view, particle));
    material.blendMode = BlendModeOf(drawSetting).value_or(Graphics::BlendMode::StandardAlpha);
    if (std::optional<Graphics::AlphaTest> test = AlphaTestOf(drawSetting, particle))
        material.alphaTest = *test;
    return material;
}

void DrawFlat(Graphics::GraphicsDevice& device, const ViewQuad& quad, const Graphics::Material& material, const EffectCamera& camera)
{
    Graphics::Quad drawn;
    for (std::size_t corner = 0; corner < Graphics::QuadCornerCount; ++corner)
    {
        Vector3 position = quad[corner].position;
        float canvasX = position.x * camera.boxToCanvasScale.x + camera.boxToCanvasOffset.x;
        float canvasY = position.y * camera.boxToCanvasScale.y + camera.boxToCanvasOffset.y;
        drawn.positions[corner] = {canvasX, canvasY};
        drawn.colors[corner] = OpaqueWhite;
        for (auto& textureCoordinates : drawn.textureCoordinates)
            textureCoordinates[corner] = quad[corner].textureCoordinate;
    }
    device.DrawQuad(drawn, material);
}

void DrawAmongModels(Graphics::GraphicsDevice& device, const ViewQuad& quad, const Graphics::Material& material, const EffectCamera& camera,
    const EffectDrawSetting& drawSetting)
{
    auto vertex = [&](std::size_t corner) {
        Graphics::MeshVertex meshVertex;
        meshVertex.position = quad[corner].position;
        meshVertex.normal = {0, 0, 1};
        meshVertex.textureCoordinate = quad[corner].textureCoordinate;
        return meshVertex;
    };
    std::array<Graphics::MeshVertex, TwoTrianglesCornerCount> corners = {vertex(0), vertex(1), vertex(2), vertex(0), vertex(2), vertex(3)};

    Graphics::MeshPlacement placement;
    placement.projection = *camera.projection;
    placement.culling = Graphics::FaceCulling::None;
    bool writesDepth = (drawSetting.flags & Formats::DrawFlag::DepthWrite) != 0;
    placement.depth = writesDepth ? Graphics::DepthMode::TestAndWrite : Graphics::DepthMode::TestOnly;
    device.DrawMesh(corners, placement, material);
}

std::vector<ViewQuad> ParticleQuads(const EmitterView& view, const Particle& particle)
{
    float width = HalfWidth(view, particle);
    float height = HalfHeight(view, particle);
    if (width < SmallestSize || height < SmallestSize)
        return {};

    const Formats::ParticleShape& shape = view.resource.shape;
    if (shape.type == Formats::ParticleDrawType::Directional)
        return {DirectionalQuad(view, particle, width, height)};
    if (shape.option == Formats::BillboardOption::AlongTheDirection)
        return {AlongTheDirectionQuad(view, particle, width, height)};
    return {FacingQuad(view, particle, width, height)};
}

void DrawQuads(const std::vector<ViewQuad>& quads, const Graphics::Material& material, const EffectCamera& camera,
    const EffectDrawSetting& drawSetting, Graphics::GraphicsDevice& device)
{
    for (const ViewQuad& quad : quads)
    {
        if (camera.projection)
            DrawAmongModels(device, quad, material, camera, drawSetting);
        else
            DrawFlat(device, quad, material, camera);
    }
}

void DrawEmitter(const Effect& effect, const Emitter& emitter, const EffectCamera& camera)
{
    const std::vector<Particle>& particles = emitter.Particles();
    if (particles.empty())
        return;

    EmitterView view = MakeEmitterView(effect, emitter, camera);
    const EffectDrawSetting& drawSetting = emitter.Resource().drawSetting;
    Graphics::GraphicsDevice& device = effect.Resources().Device();

    if (emitter.Resource().shape.type == Formats::ParticleDrawType::SmoothStripe)
    {
        DrawQuads(SmoothStripeQuads(view, particles), ParticleMaterial(view, particles.back()), camera, drawSetting, device);
        return;
    }

    bool oldestFirst = (drawSetting.flags & Formats::DrawFlag::OldestFirst) != 0;
    for (std::size_t index = 0; index < particles.size(); ++index)
    {
        const Particle& particle = oldestFirst ? particles[index] : particles[particles.size() - 1 - index];
        DrawQuads(ParticleQuads(view, particle), ParticleMaterial(view, particle), camera, drawSetting, device);
    }
}

}

std::optional<Graphics::BlendMode> BlendModeOf(const EffectDrawSetting& drawSetting)
{
    if (drawSetting.blendType == NoBlend)
        return Graphics::BlendMode::Replace;
    if (drawSetting.blendType != Blend)
        return std::nullopt;

    bool fromSourceAlpha = drawSetting.blendSource == FactorSourceAlpha;
    if (fromSourceAlpha && drawSetting.blendDestination == FactorInverseSourceAlpha)
        return Graphics::BlendMode::StandardAlpha;
    if (fromSourceAlpha && drawSetting.blendDestination == FactorOne)
        return Graphics::BlendMode::AddSourceAlpha;
    if (drawSetting.blendSource == FactorOne && drawSetting.blendDestination == FactorZero)
        return Graphics::BlendMode::Replace;
    return std::nullopt;
}

// newS = factor * s' (+ factor when reversed), s' = the coordinate moved, turned and scaled around the middle of the picture.
Graphics::TextureMatrix ParticleTextureMatrix(const Formats::ParticleTextureTransform& transform, std::uint8_t wrapBits,
    std::uint8_t reverseBits)
{
    constexpr float Middle = 0.5f;

    float cosine = std::cos(transform.rotation);
    float sine = std::sin(transform.rotation);
    float scaleS = transform.scale.x;
    float scaleT = transform.scale.y;

    // Turned about the middle: rows (cos, -sin) and (sin, cos), with the middle kept in place; then scaled about the middle.
    std::array<float, 3> rowS = {scaleS * cosine, -scaleS * sine, 0};
    std::array<float, 3> rowT = {scaleT * sine, scaleT * cosine, 0};
    float movedS = transform.translation.x - Middle;
    float movedT = transform.translation.y - Middle;
    rowS[TextureTranslationColumn] = scaleS * (cosine * movedS - sine * movedT) + Middle;
    rowT[TextureTranslationColumn] = scaleT * (sine * movedS + cosine * movedT) + Middle;

    bool mirrorsS = (wrapBits & WrapMask) == MirrorWrap;
    bool mirrorsT = ((wrapBits >> VerticalWrapShift) & WrapMask) == MirrorWrap;
    float factorS = mirrorsS ? MirrorRepeat : 1.0f;
    float factorT = mirrorsT ? MirrorRepeat : 1.0f;
    bool reversesS = (reverseBits & ReversesHorizontally) != 0;
    bool reversesT = (reverseBits & ReversesVertically) != 0;

    float signedFactorS = reversesS ? -factorS : factorS;
    float signedFactorT = reversesT ? -factorT : factorT;

    Graphics::TextureMatrix matrix;
    for (std::size_t column = 0; column < rowS.size(); ++column)
    {
        matrix.rows[0][column] = signedFactorS * rowS[column];
        matrix.rows[1][column] = signedFactorT * rowT[column];
    }
    if (reversesS)
        matrix.rows[0][TextureTranslationColumn] += factorS;
    if (reversesT)
        matrix.rows[1][TextureTranslationColumn] += factorT;
    return matrix;
}

void DrawEffect(const Effect& effect, const EffectCamera& camera)
{
    for (const Emitter* emitter : effect.EmittersToDraw())
        DrawEmitter(effect, *emitter, camera);
}

}
