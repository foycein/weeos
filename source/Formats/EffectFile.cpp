#include "Formats/EffectFile.h"

#include "Formats/ByteReader.h"
#include "Formats/FormatError.h"
#include "Formats/TextureDecoder.h"
#include "Formats/TextureFormat.h"

#include <bit>
#include <format>

namespace Formats
{

namespace
{

constexpr std::string_view EffectFileTag = "REFF";
constexpr std::string_view EffectTextureFileTag = "REFT";
constexpr std::uint16_t ByteOrderMark = 0xFEFF;

// The section header: the offset of the entry table is counted from here.
constexpr std::size_t TableOffsetPlace = 0x18;
constexpr std::size_t ProjectNameLengthPlace = 0x24;
constexpr std::size_t ProjectNamePlace = 0x28;
constexpr std::size_t EntryTableHeaderSize = 8;

// The emitter block starts with a word the library fills at run time and the size of the emitter description.
constexpr std::size_t EmitterDescriptionPlace = 8;
constexpr std::size_t Version5ShapePlace = 0x94;
constexpr std::size_t Version5PivotPlace = 0x9C;
constexpr std::size_t Version5DrawSettingPlace = 0xA8;
constexpr std::size_t Version11DrawSettingPlace = 0x9C;

constexpr float PercentPerUnit = 100;
constexpr std::uint32_t Version5HeightRuleMask = 0x18;
constexpr std::uint16_t Version11HeightRuleMask = 0x6000;
constexpr int Version11HeightRuleShift = 13;
constexpr std::uint8_t NoStageTexture = 0xFF;

// The particle parameters: the texture names come after the fixed fields.
constexpr std::size_t Version5TextureNamesPlace = 0x7A;
constexpr std::size_t Version11TextureNamesPlace = 0x88;

// Animation blocks.
constexpr std::uint8_t KeyCurveTag = 0xAC;
constexpr std::uint8_t BakedCurveTag = 0xAB;
constexpr std::size_t AnimationHeaderSize = 0x20;
constexpr std::size_t TableHeaderSize = 4;
constexpr std::size_t Version5KeyHeaderSize = 8;
constexpr std::size_t Version5KeyRandomPlace = 6;
constexpr std::size_t Version11KeyHeaderSize = 12;
constexpr std::size_t Version11KeyInterpolationPlace = 4;
constexpr std::size_t RotationComponentCount = 3;
constexpr std::size_t RotationRangeEntrySize = 0x1C;
constexpr std::size_t Version5ChildValueSize = 10;
constexpr std::size_t Version11ChildValueSize = 12;
constexpr std::size_t TextureKeyValueSize = 4;
constexpr std::size_t Version11FieldHeaderSize = 4;

// The texture header of a .breft entry: version 11 adds a filter and a level of detail bias and pads the header to 0x40 bytes.
constexpr std::size_t Version5TextureHeaderSize = 0x20;
constexpr std::size_t Version11TextureHeaderSize = 0x40;
constexpr std::size_t TextureFormatPlace = 0x0C;

struct TableEntry
{
    std::string name;
    std::uint32_t offset;
    std::uint32_t size;
};

std::string ReadSizedName(ByteReader& reader)
{
    std::uint16_t length = reader.ReadUint16();
    return reader.ReadText(length);
}

std::size_t CheckFrame(ByteReader& reader, std::string_view tag, std::uint16_t& version)
{
    if (reader.ReadText(tag.size()) != tag)
        reader.Fail(std::format("the file does not start with {}", tag));
    if (reader.ReadUint16() != ByteOrderMark)
        reader.Fail("the byte order mark is wrong");

    version = reader.ReadUint16();
    if (version != EffectFileVersion5 && version != EffectFileVersion11)
        reader.Fail(std::format("version {} is not known", version));

    reader.MoveTo(TableOffsetPlace);
    return TableOffsetPlace + reader.ReadUint32();
}

std::vector<TableEntry> ReadEntryTable(ByteReader& reader, std::size_t tableOffset)
{
    reader.MoveTo(tableOffset + sizeof(std::uint32_t));
    std::uint16_t count = reader.ReadUint16();
    reader.MoveTo(tableOffset + EntryTableHeaderSize);

    std::vector<TableEntry> entries;
    for (std::uint16_t number = 0; number < count; ++number)
    {
        TableEntry entry;
        entry.name = ReadSizedName(reader);
        entry.offset = reader.ReadUint32();
        entry.size = reader.ReadUint32();
        entries.push_back(entry);
    }
    return entries;
}

float ReadPercent(ByteReader& reader)
{
    return static_cast<std::int8_t>(reader.ReadByte()) / PercentPerUnit;
}

EmitterSettings ReadEmitterSettings(ByteReader& reader, std::size_t block)
{
    reader.MoveTo(block + EmitterDescriptionPlace);

    EmitterSettings settings;
    settings.commonFlags = reader.ReadUint32();
    settings.emitFlags = reader.ReadUint32();
    settings.form = static_cast<EmitterForm>(settings.emitFlags & EmitFlag::FormMask);
    settings.emitterLife = reader.ReadUint16();
    settings.particleLife = reader.ReadUint16();
    settings.particleLifeRandomPercent = static_cast<std::int8_t>(reader.ReadByte());
    reader.Skip(1);
    settings.emitIntervalRandomPercent = static_cast<std::int8_t>(reader.ReadByte());
    settings.emitCountRandomPercent = static_cast<std::int8_t>(reader.ReadByte());
    settings.emitCount = reader.ReadFloat();
    settings.emitStartDelay = reader.ReadUint16();
    settings.preRollSteps = reader.ReadUint16();
    settings.emitInterval = reader.ReadUint16();
    reader.Skip(2);

    for (float& parameter : settings.formParameters)
        parameter = reader.ReadFloat();

    settings.fixedCount = reader.ReadUint16();
    settings.initialVelocityRandomPercent = reader.ReadByte();
    settings.momentumRandomPercent = reader.ReadByte();
    settings.radiationSpeed = reader.ReadFloat();
    settings.yAxisSpeed = reader.ReadFloat();
    settings.randomDirectionSpeed = reader.ReadFloat();
    settings.normalSpeed = reader.ReadFloat();
    settings.normalDiffusion = reader.ReadFloat();
    settings.specialDirectionSpeed = reader.ReadFloat();
    settings.specialDirectionDiffusion = reader.ReadFloat();
    settings.specialDirectionAngles = ReadVector3(reader);
    settings.scale = ReadVector3(reader);
    settings.rotation = ReadVector3(reader);
    settings.translation = ReadVector3(reader);
    reader.Skip(4);
    settings.randomSeed = static_cast<std::uint16_t>(reader.ReadUint32());
    return settings;
}

EffectStage ReadStageCombiner(ByteReader& reader, std::size_t drawSetting, std::size_t stage, std::size_t colorInputs, std::size_t colorOperation,
    std::size_t alphaInputs, std::size_t alphaOperation)
{
    constexpr std::size_t InputsPerStage = 4;
    constexpr std::size_t OperationBytesPerStage = 5;

    EffectStage combiner;
    reader.MoveTo(drawSetting + colorInputs + stage * InputsPerStage);
    combiner.colorInputs = reader.ReadByteArray<4>();
    reader.MoveTo(drawSetting + colorOperation + stage * OperationBytesPerStage);
    combiner.colorOperation = reader.ReadByteArray<5>();
    reader.MoveTo(drawSetting + alphaInputs + stage * InputsPerStage);
    combiner.alphaInputs = reader.ReadByteArray<4>();
    reader.MoveTo(drawSetting + alphaOperation + stage * OperationBytesPerStage);
    combiner.alphaOperation = reader.ReadByteArray<5>();
    return combiner;
}

ParticleColorSource ReadColorSource(ByteReader& reader)
{
    constexpr std::uint8_t HighestSource = 6;

    std::uint8_t number = reader.ReadByte();
    if (number > HighestSource)
        reader.Fail(std::format("the color source {} is not known", number));
    return static_cast<ParticleColorSource>(number);
}

void ReadAlphaTestAndCount(ByteReader& reader, std::size_t drawSetting, EffectDrawSetting& setting, std::size_t& stageCount)
{
    reader.MoveTo(drawSetting);
    setting.flags = reader.ReadUint16();
    setting.firstAlphaComparison = reader.ReadByte();
    setting.secondAlphaComparison = reader.ReadByte();
    setting.alphaComparisonJoin = reader.ReadByte();
    stageCount = reader.ReadByte();
    if (stageCount == 0 || stageCount > EffectStageCount)
        reader.Fail(std::format("{} stages", stageCount));
}

// Version 5: the alpha operation of a stage is the same as its color operation (the library passes the color bytes to both),
// stage 0 reads texture 1 and the other stages texture 2, and one group of sources fills both color and alpha.
EffectDrawSetting ReadDrawSettingVersion5(ByteReader& reader, std::size_t drawSetting)
{
    constexpr std::size_t ColorInputs = 0x08;
    constexpr std::size_t ColorOperation = 0x18;
    constexpr std::size_t AlphaInputs = 0x2C;
    constexpr std::size_t ConstantSelections = 0x50;
    constexpr std::size_t ConstantAlphaSelections = 0x54;
    constexpr std::size_t Blend = 0x58;
    constexpr std::size_t RegisterSources = 0x5D;
    constexpr std::size_t Flicker = 0x65;
    constexpr std::size_t Lighting = 0x6B;
    constexpr std::size_t IndirectMatrix = 0x84;
    constexpr std::size_t IndirectScale = 0x9C;
    constexpr std::size_t IndirectStage = 0x07;

    EffectDrawSetting setting;
    std::size_t stageCount = 0;
    ReadAlphaTestAndCount(reader, drawSetting, setting, stageCount);

    for (std::size_t stage = 0; stage < stageCount; ++stage)
    {
        EffectStage combiner = ReadStageCombiner(reader, drawSetting, stage, ColorInputs, ColorOperation, AlphaInputs, ColorOperation);
        bool readsTexture1 = stage == 0 && (setting.flags & DrawFlag::UsesTexture1) != 0;
        bool readsTexture2 = stage > 0 && (setting.flags & DrawFlag::UsesTexture2) != 0;
        combiner.textureNumber = NoStageTexture;
        if (readsTexture1)
            combiner.textureNumber = 0;
        if (readsTexture2)
            combiner.textureNumber = 1;

        reader.MoveTo(drawSetting + ConstantSelections + stage);
        combiner.constantColorSelection = reader.ReadByte();
        reader.MoveTo(drawSetting + ConstantAlphaSelections + stage);
        combiner.constantAlphaSelection = reader.ReadByte();
        setting.stages.push_back(combiner);
    }

    reader.MoveTo(drawSetting + IndirectStage);
    std::uint8_t indirectStage = reader.ReadByte();
    if ((setting.flags & DrawFlag::UsesIndirectTexture) != 0)
        setting.indirectStageMask = static_cast<std::uint8_t>(1u << indirectStage);

    reader.MoveTo(drawSetting + Blend);
    setting.blendType = reader.ReadByte();
    setting.blendSource = reader.ReadByte();
    setting.blendDestination = reader.ReadByte();

    reader.MoveTo(drawSetting + RegisterSources);
    for (ParticleColorSource& source : setting.registerColors)
        source = ReadColorSource(reader);
    for (ParticleColorSource& source : setting.constantColors)
        source = ReadColorSource(reader);
    setting.registerAlphas = setting.registerColors;
    setting.constantAlphas = setting.constantColors;

    reader.MoveTo(drawSetting + Flicker);
    setting.flickerType = reader.ReadByte();
    reader.MoveTo(drawSetting + Lighting);
    setting.lightingType = reader.ReadByte();
    setting.ambientColor = ReadRgba8Color(reader);

    reader.MoveTo(drawSetting + IndirectMatrix);
    for (auto& row : setting.indirectMatrix)
        for (float& number : row)
            number = reader.ReadFloat();
    reader.MoveTo(drawSetting + IndirectScale);
    setting.indirectScaleExponent = static_cast<std::int8_t>(reader.ReadByte());
    return setting;
}

EffectDrawSetting ReadDrawSettingVersion11(ByteReader& reader, std::size_t drawSetting)
{
    constexpr std::size_t IndirectStages = 0x07;
    constexpr std::size_t StageTextures = 0x08;
    constexpr std::size_t ColorInputs = 0x0C;
    constexpr std::size_t ColorOperation = 0x1C;
    constexpr std::size_t AlphaInputs = 0x30;
    constexpr std::size_t AlphaOperation = 0x40;
    constexpr std::size_t ConstantSelections = 0x54;
    constexpr std::size_t ConstantAlphaSelections = 0x58;
    constexpr std::size_t Blend = 0x5C;
    constexpr std::size_t ColorSources = 0x61;
    constexpr std::size_t AlphaSources = 0x69;
    constexpr std::size_t Flicker = 0x71;
    constexpr std::size_t Lighting = 0x77;
    constexpr std::size_t IndirectMatrix = 0x90;
    constexpr std::size_t IndirectScale = 0xA8;

    EffectDrawSetting setting;
    std::size_t stageCount = 0;
    ReadAlphaTestAndCount(reader, drawSetting, setting, stageCount);

    for (std::size_t stage = 0; stage < stageCount; ++stage)
    {
        EffectStage combiner = ReadStageCombiner(reader, drawSetting, stage, ColorInputs, ColorOperation, AlphaInputs, AlphaOperation);
        reader.MoveTo(drawSetting + StageTextures + stage);
        combiner.textureNumber = reader.ReadByte();
        reader.MoveTo(drawSetting + ConstantSelections + stage);
        combiner.constantColorSelection = reader.ReadByte();
        reader.MoveTo(drawSetting + ConstantAlphaSelections + stage);
        combiner.constantAlphaSelection = reader.ReadByte();
        setting.stages.push_back(combiner);
    }

    reader.MoveTo(drawSetting + IndirectStages);
    setting.indirectStageMask = reader.ReadByte();
    if ((setting.flags & DrawFlag::UsesIndirectTexture) == 0)
        setting.indirectStageMask = 0;

    reader.MoveTo(drawSetting + Blend);
    setting.blendType = reader.ReadByte();
    setting.blendSource = reader.ReadByte();
    setting.blendDestination = reader.ReadByte();

    reader.MoveTo(drawSetting + ColorSources);
    for (ParticleColorSource& source : setting.registerColors)
        source = ReadColorSource(reader);
    for (ParticleColorSource& source : setting.constantColors)
        source = ReadColorSource(reader);
    reader.MoveTo(drawSetting + AlphaSources);
    for (ParticleColorSource& source : setting.registerAlphas)
        source = ReadColorSource(reader);
    for (ParticleColorSource& source : setting.constantAlphas)
        source = ReadColorSource(reader);

    reader.MoveTo(drawSetting + Flicker);
    setting.flickerType = reader.ReadByte();
    reader.MoveTo(drawSetting + Lighting);
    setting.lightingType = reader.ReadByte();
    setting.ambientColor = ReadRgba8Color(reader);

    reader.MoveTo(drawSetting + IndirectMatrix);
    for (auto& row : setting.indirectMatrix)
        for (float& number : row)
            number = reader.ReadFloat();
    reader.MoveTo(drawSetting + IndirectScale);
    setting.indirectScaleExponent = static_cast<std::int8_t>(reader.ReadByte());
    return setting;
}

void ReadShapeKind(ByteReader& reader, ParticleShape& shape)
{
    constexpr std::uint8_t HighestDrawType = 6;

    std::uint8_t type = reader.ReadByte();
    if (type > HighestDrawType)
        type = static_cast<std::uint8_t>(ParticleDrawType::Billboard);
    shape.type = static_cast<ParticleDrawType>(type);
    shape.option = reader.ReadByte();
    shape.direction = reader.ReadByte();
    shape.axis = reader.ReadByte();
    shape.stretchesWithSpeed = reader.ReadByte();
    shape.stripeSubdivisions = reader.ReadByte();
    shape.stripeFlags = reader.ReadByte();
}

ParticleShape ReadShapeVersion5(ByteReader& reader, std::size_t block, std::uint32_t commonFlags)
{
    ParticleShape shape;
    reader.MoveTo(block + Version5ShapePlace);
    ReadShapeKind(reader, shape);
    reader.MoveTo(block + Version5PivotPlace);
    shape.pivot.x = ReadPercent(reader);
    shape.pivot.y = ReadPercent(reader);

    constexpr int Version5HeightRuleShift = 3;
    shape.heightRule = static_cast<ParticleHeightRule>((commonFlags & Version5HeightRuleMask) >> Version5HeightRuleShift);
    return shape;
}

ParticleShape ReadShapeVersion11(ByteReader& reader, std::size_t drawSetting, std::uint16_t drawFlags)
{
    constexpr std::size_t Pivot = 0xA9;
    constexpr std::size_t Kind = 0xAC;

    ParticleShape shape;
    reader.MoveTo(drawSetting + Pivot);
    shape.pivot.x = ReadPercent(reader);
    shape.pivot.y = ReadPercent(reader);
    reader.MoveTo(drawSetting + Kind);
    ReadShapeKind(reader, shape);
    shape.heightRule = static_cast<ParticleHeightRule>((drawFlags & Version11HeightRuleMask) >> Version11HeightRuleShift);
    return shape;
}

ParticleParameters ReadParticleParameters(ByteReader& reader, std::size_t parameters, std::uint16_t version)
{
    reader.MoveTo(parameters);

    ParticleParameters particle;
    for (Rgba8Color& color : particle.colors)
        color = ReadRgba8Color(reader);
    particle.size = ReadVector2(reader);
    particle.scale = ReadVector2(reader);
    particle.rotation = ReadVector3(reader);
    for (ParticleTextureTransform& transform : particle.textureTransforms)
        transform.scale = ReadVector2(reader);
    for (ParticleTextureTransform& transform : particle.textureTransforms)
        transform.rotation = reader.ReadFloat();
    for (ParticleTextureTransform& transform : particle.textureTransforms)
        transform.translation = ReadVector2(reader);

    constexpr std::size_t RunTimeTexturePointersSize = 12;
    reader.Skip(RunTimeTexturePointersSize);
    particle.textureWraps = reader.ReadUint16();
    particle.textureReverses = reader.ReadByte();
    particle.firstAlphaReference = reader.ReadByte();
    particle.secondAlphaReference = reader.ReadByte();

    std::size_t namesPlace = parameters + Version5TextureNamesPlace;
    if (version == EffectFileVersion5)
    {
        particle.sizeRandomPercent = static_cast<std::int8_t>(reader.ReadByte());
    }
    else
    {
        particle.rotationOffsetRandomPercent = reader.ReadByteArray<3>();
        particle.rotationOffset = ReadVector3(reader);
        namesPlace = parameters + Version11TextureNamesPlace;
    }

    reader.MoveTo(namesPlace);
    for (std::string& name : particle.textureNames)
        name = ReadSizedName(reader);
    return particle;
}

// The target that comes this many places after the first one in the list of EffectAnimationTarget.
EffectAnimationTarget TargetAfter(EffectAnimationTarget first, int places)
{
    return static_cast<EffectAnimationTarget>(static_cast<int>(first) + places);
}

EffectAnimationTarget Version5Target(ByteReader& reader, std::uint8_t number)
{
    constexpr std::uint8_t LastColorTarget = 0x0C;
    constexpr std::uint8_t FirstPatternTarget = 0x0E;
    constexpr std::uint8_t LastTextureTarget = 0x19;
    constexpr std::uint8_t ChildTarget = 0x1A;
    constexpr std::uint8_t FirstFieldTarget = 0x20;
    constexpr std::uint8_t FirstEmitterTarget = 0x40;
    constexpr std::uint8_t LastEmitterTarget = 0x49;
    constexpr std::uint8_t FieldOfSpin = 0x26;
    constexpr std::uint8_t FieldOfTail = 0x27;

    if (number <= LastColorTarget)
        return static_cast<EffectAnimationTarget>(number);
    if (number >= FirstPatternTarget && number <= LastTextureTarget)
        return TargetAfter(EffectAnimationTarget::Texture1Pattern, number - FirstPatternTarget);
    if (number == ChildTarget)
        return EffectAnimationTarget::Child;
    if (number >= FirstFieldTarget && number <= static_cast<std::uint8_t>(FirstFieldTarget + 4))
        return TargetAfter(EffectAnimationTarget::GravityField, number - FirstFieldTarget);
    if (number == FieldOfSpin)
        return EffectAnimationTarget::SpinField;
    if (number == FieldOfTail)
        return EffectAnimationTarget::TailField;
    if (number >= FirstEmitterTarget && number <= LastEmitterTarget)
        return TargetAfter(EffectAnimationTarget::EmitterFormParameters, number - FirstEmitterTarget);

    reader.Fail(std::format("the animation target {:#x} is not known", number));
}

// Version 11 names most targets by their place in the particle parameters.
EffectAnimationTarget Version11ParameterTarget(ByteReader& reader, std::uint8_t place)
{
    switch (place)
    {
    case 0x00: return EffectAnimationTarget::Color1PrimaryRgb;
    case 0x03: return EffectAnimationTarget::Color1PrimaryAlpha;
    case 0x04: return EffectAnimationTarget::Color1SecondaryRgb;
    case 0x07: return EffectAnimationTarget::Color1SecondaryAlpha;
    case 0x08: return EffectAnimationTarget::Color2PrimaryRgb;
    case 0x0B: return EffectAnimationTarget::Color2PrimaryAlpha;
    case 0x0C: return EffectAnimationTarget::Color2SecondaryRgb;
    case 0x0F: return EffectAnimationTarget::Color2SecondaryAlpha;
    case 0x10: return EffectAnimationTarget::Size;
    case 0x18: return EffectAnimationTarget::Scale;
    case 0x2C: return EffectAnimationTarget::Texture1Scale;
    case 0x34: return EffectAnimationTarget::Texture2Scale;
    case 0x3C: return EffectAnimationTarget::IndirectTextureScale;
    case 0x44: return EffectAnimationTarget::Texture1Rotation;
    case 0x48: return EffectAnimationTarget::Texture2Rotation;
    case 0x4C: return EffectAnimationTarget::IndirectTextureRotation;
    case 0x50: return EffectAnimationTarget::Texture1Translation;
    case 0x58: return EffectAnimationTarget::Texture2Translation;
    case 0x60: return EffectAnimationTarget::IndirectTextureTranslation;
    case 0x68: return EffectAnimationTarget::Texture1Pattern;
    case 0x6C: return EffectAnimationTarget::Texture2Pattern;
    case 0x70: return EffectAnimationTarget::IndirectTexturePattern;
    case 0x77: return EffectAnimationTarget::FirstAlphaReference;
    case 0x78: return EffectAnimationTarget::SecondAlphaReference;
    default: break;
    }
    reader.Fail(std::format("the animation target {:#x} is not known", place));
}

EffectAnimationTarget Version11FieldTarget(ByteReader& reader, std::uint8_t type)
{
    switch (type)
    {
    case 0: return EffectAnimationTarget::GravityField;
    case 1: return EffectAnimationTarget::SpeedField;
    case 2: return EffectAnimationTarget::MagnetField;
    case 3: return EffectAnimationTarget::NewtonField;
    case 4: return EffectAnimationTarget::VortexField;
    case 6: return EffectAnimationTarget::SpinField;
    case 7: return EffectAnimationTarget::TailField;
    default: break;
    }
    reader.Fail(std::format("the field {} is not known", type));
}

struct AnimationHeader
{
    std::uint8_t tag;
    std::uint8_t target;
    std::uint8_t kind;
    std::uint8_t componentMask;
    std::uint32_t keyTableSize;
    std::uint32_t rangeTableSize;
    std::uint32_t randomTableSize;
    std::uint32_t nameTableSize;
    std::uint32_t infoSize;
};

AnimationValueKind ValueKindOf(ByteReader& reader, const AnimationHeader& header, std::uint16_t version)
{
    constexpr std::uint8_t BytesKind = 0;
    constexpr std::uint8_t FloatsKind = 3;
    constexpr std::uint8_t TexturePatternKind = 4;
    constexpr std::uint8_t ChildKind = 5;
    constexpr std::uint8_t RotationKind = 6;
    constexpr std::uint8_t Version11FieldKind = 7;
    constexpr std::uint8_t Version5FirstFieldTarget = 0x20;
    constexpr std::uint8_t Version5LastFieldTarget = 0x27;

    switch (header.kind)
    {
    case BytesKind: return AnimationValueKind::Bytes;
    case TexturePatternKind: return AnimationValueKind::TexturePattern;
    case ChildKind: return AnimationValueKind::Child;
    case RotationKind: return AnimationValueKind::Rotation;
    default: break;
    }

    bool isFieldTarget = header.target >= Version5FirstFieldTarget && header.target <= Version5LastFieldTarget;
    bool isVersion5Field = version == EffectFileVersion5 && isFieldTarget;
    if (header.kind == FloatsKind && isVersion5Field)
        return AnimationValueKind::Field;
    if (header.kind == FloatsKind)
        return AnimationValueKind::Floats;
    if (header.kind == Version11FieldKind && version == EffectFileVersion11)
        return AnimationValueKind::Field;

    reader.Fail(std::format("the animation kind {} is not known", header.kind));
}

std::size_t ValueSizeOf(const EffectAnimation& animation, std::uint16_t version)
{
    switch (animation.kind)
    {
    case AnimationValueKind::Bytes:
        return (animation.componentCount + 1) / 2 * 2;
    case AnimationValueKind::TexturePattern:
        return TextureKeyValueSize;
    case AnimationValueKind::Child:
        return version == EffectFileVersion5 ? Version5ChildValueSize : Version11ChildValueSize;
    default:
        return animation.componentCount * sizeof(float);
    }
}

ChildRequest ReadChildRequest(ByteReader& reader, std::uint16_t version)
{
    constexpr std::uint8_t MakesEmitter = 1;

    ChildRequest child;
    child.velocityPercent = reader.ReadInt16();
    child.sizePercent = reader.ReadByte();
    child.alphaPercent = reader.ReadByte();
    child.colorPercent = reader.ReadByte();
    child.drawPriority = reader.ReadByte();
    child.makesEmitter = reader.ReadByte() == MakesEmitter;
    reader.Skip(1);
    if (version == EffectFileVersion11)
        reader.Skip(2);
    child.nameIndex = reader.ReadUint16();
    return child;
}

void ReadKeyValue(ByteReader& reader, const EffectAnimation& animation, std::uint16_t version, AnimationKey& key)
{
    std::size_t valueStart = reader.Position();
    if (key.randomFlags != 0)
        key.rangeIndex = reader.ReadUint16();
    reader.MoveTo(valueStart);

    switch (animation.kind)
    {
    case AnimationValueKind::Bytes:
        for (std::size_t component = 0; component < animation.componentCount; ++component)
            key.values.push_back(reader.ReadByte());
        break;
    case AnimationValueKind::TexturePattern:
        key.textureWrap = reader.ReadByte();
        key.textureReverse = reader.ReadByte();
        key.textureNameIndex = reader.ReadUint16();
        break;
    case AnimationValueKind::Child:
        key.child = ReadChildRequest(reader, version);
        break;
    default:
        for (std::size_t component = 0; component < animation.componentCount; ++component)
            key.values.push_back(reader.ReadFloat());
        break;
    }
}

void ReadKeys(ByteReader& reader, std::size_t keyTable, EffectAnimation& animation, std::uint16_t version)
{
    reader.MoveTo(keyTable);
    std::uint16_t count = reader.ReadUint16();
    reader.Skip(2);

    std::size_t headerSize = version == EffectFileVersion5 ? Version5KeyHeaderSize : Version11KeyHeaderSize;
    std::size_t keySize = headerSize + ValueSizeOf(animation, version);
    for (std::uint16_t number = 0; number < count; ++number)
    {
        std::size_t keyStart = keyTable + TableHeaderSize + number * keySize;
        reader.MoveTo(keyStart);

        AnimationKey key;
        key.frame = reader.ReadUint16();
        if (version == EffectFileVersion5)
        {
            key.interpolation = static_cast<std::uint8_t>(reader.ReadUint16());
            reader.MoveTo(keyStart + Version5KeyRandomPlace);
            key.randomFlags = reader.ReadByte();
        }
        else
        {
            key.randomFlags = reader.ReadByte();
            reader.MoveTo(keyStart + Version11KeyInterpolationPlace);
            key.interpolation = reader.ReadByte();
        }

        reader.MoveTo(keyStart + headerSize);
        ReadKeyValue(reader, animation, version, key);
        animation.keys.push_back(key);
    }
}

void ReadBakedValues(ByteReader& reader, std::size_t values, EffectAnimation& animation)
{
    reader.MoveTo(values);
    std::size_t count = animation.frameCount * animation.componentCount;
    for (std::size_t number = 0; number < count; ++number)
    {
        if (animation.kind == AnimationValueKind::Bytes)
            animation.bakedValues.push_back(reader.ReadByte());
        else
            animation.bakedValues.push_back(reader.ReadFloat());
    }
}

RandomRange ReadRange(ByteReader& reader, const EffectAnimation& animation)
{
    RandomRange range;
    std::size_t componentCount = animation.componentCount;
    if (animation.kind == AnimationValueKind::Rotation)
        componentCount = RotationComponentCount;

    for (std::size_t component = 0; component < componentCount; ++component)
    {
        if (animation.kind == AnimationValueKind::Bytes)
        {
            range.bases.push_back(reader.ReadByte());
            range.steps.push_back(reader.ReadByte());
            continue;
        }
        range.bases.push_back(reader.ReadFloat());
        range.steps.push_back(reader.ReadFloat());
    }

    if (animation.kind == AnimationValueKind::Rotation)
    {
        range.hasRandomSign = reader.ReadByte() != 0;
        reader.Skip(3);
    }
    return range;
}

std::size_t RangeEntrySize(const EffectAnimation& animation)
{
    if (animation.kind == AnimationValueKind::Rotation)
        return RotationRangeEntrySize;
    if (animation.kind == AnimationValueKind::Bytes)
        return animation.componentCount * 2;
    return animation.componentCount * 2 * sizeof(float);
}

std::vector<RandomRange> ReadRangeTable(ByteReader& reader, std::size_t table, std::uint32_t tableSize, const EffectAnimation& animation)
{
    std::vector<RandomRange> ranges;
    bool hasRanges = animation.kind == AnimationValueKind::Bytes || animation.kind == AnimationValueKind::Floats ||
        animation.kind == AnimationValueKind::Rotation;
    if (tableSize < TableHeaderSize || !hasRanges)
        return ranges;

    reader.MoveTo(table);
    std::uint16_t count = reader.ReadUint16();
    for (std::uint16_t number = 0; number < count; ++number)
    {
        reader.MoveTo(table + TableHeaderSize + number * RangeEntrySize(animation));
        ranges.push_back(ReadRange(reader, animation));
    }
    return ranges;
}

std::vector<std::string> ReadNameTable(ByteReader& reader, std::size_t table, std::uint32_t tableSize)
{
    std::vector<std::string> names;
    if (tableSize < TableHeaderSize)
        return names;

    reader.MoveTo(table);
    std::uint16_t count = reader.ReadUint16();
    reader.MoveTo(table + TableHeaderSize + count * sizeof(std::uint32_t));
    for (std::uint16_t number = 0; number < count; ++number)
        names.push_back(ReadSizedName(reader));
    return names;
}

std::size_t Version5FieldFloatCount(EffectAnimationTarget target)
{
    switch (target)
    {
    case EffectAnimationTarget::GravityField: return 4;
    case EffectAnimationTarget::SpeedField: return 1;
    case EffectAnimationTarget::MagnetField: return 4;
    case EffectAnimationTarget::NewtonField: return 5;
    case EffectAnimationTarget::VortexField: return 6;
    case EffectAnimationTarget::SpinField: return 4;
    default: return 1;
    }
}

FieldSettings ReadFieldSettings(ByteReader& reader, std::size_t info, std::uint32_t infoSize, EffectAnimationTarget target, std::uint16_t version)
{
    FieldSettings field;
    if (infoSize == 0)
        return field;

    reader.MoveTo(info);
    if (version == EffectFileVersion11)
    {
        field.space = reader.ReadByte();
        field.mode = reader.ReadByte();
        reader.MoveTo(info + Version11FieldHeaderSize);
        while (reader.Position() + sizeof(float) <= info + infoSize)
            field.values.push_back(reader.ReadFloat());
        return field;
    }

    std::size_t floatCount = Version5FieldFloatCount(target);
    for (std::size_t number = 0; number < floatCount; ++number)
        field.values.push_back(reader.ReadFloat());
    if (reader.Position() + 2 <= info + infoSize)
    {
        field.space = reader.ReadByte();
        field.mode = reader.ReadByte();
    }
    return field;
}

EffectAnimation ReadAnimation(ByteReader& reader, std::size_t start, std::uint16_t version, bool isEmitterAnimation)
{
    reader.MoveTo(start);
    AnimationHeader header{};
    header.tag = reader.ReadByte();
    header.target = reader.ReadByte();
    header.kind = reader.ReadByte();
    header.componentMask = reader.ReadByte();

    EffectAnimation animation;
    animation.flags = reader.ReadByte();
    animation.loopCount = reader.ReadByte();
    animation.randomSeed = reader.ReadUint16();
    animation.frameCount = reader.ReadUint16();
    reader.Skip(2);
    header.keyTableSize = reader.ReadUint32();
    header.rangeTableSize = reader.ReadUint32();
    header.randomTableSize = reader.ReadUint32();
    header.nameTableSize = reader.ReadUint32();
    header.infoSize = reader.ReadUint32();

    if (header.tag != KeyCurveTag && header.tag != BakedCurveTag)
        reader.Fail(std::format("an animation starts with {:#x}", header.tag));
    animation.isBaked = header.tag == BakedCurveTag;
    animation.componentCount = std::popcount(header.componentMask);

    // Version 11's emitter animations are not read: none of the files plays one without the owner's server (the lobby).
    if (isEmitterAnimation && version == EffectFileVersion11)
    {
        animation.target = EffectAnimationTarget::EmitterUnnamed;
        return animation;
    }

    animation.kind = ValueKindOf(reader, header, version);
    if (version == EffectFileVersion5)
        animation.target = Version5Target(reader, header.target);
    else if (animation.kind == AnimationValueKind::Field)
        animation.target = Version11FieldTarget(reader, header.target);
    else if (animation.kind == AnimationValueKind::Child)
        animation.target = EffectAnimationTarget::Child;
    else if (animation.kind == AnimationValueKind::Rotation)
        animation.target = EffectAnimationTarget::Rotation;
    else
        animation.target = Version11ParameterTarget(reader, header.target);

    if (animation.kind == AnimationValueKind::Rotation)
        animation.componentCount = RotationComponentCount;

    std::size_t keyTable = start + AnimationHeaderSize;
    std::size_t rangeTable = keyTable + header.keyTableSize;
    std::size_t randomTable = rangeTable + header.rangeTableSize;
    std::size_t nameTable = randomTable + header.randomTableSize;
    std::size_t info = nameTable + header.nameTableSize;

    if (animation.isBaked)
        ReadBakedValues(reader, keyTable, animation);
    else if (header.keyTableSize >= TableHeaderSize)
        ReadKeys(reader, keyTable, animation, version);

    animation.rangeTable = ReadRangeTable(reader, rangeTable, header.rangeTableSize, animation);
    animation.randomTable = ReadRangeTable(reader, randomTable, header.randomTableSize, animation);
    animation.names = ReadNameTable(reader, nameTable, header.nameTableSize);
    if (animation.kind == AnimationValueKind::Field)
        animation.field = ReadFieldSettings(reader, info, header.infoSize, animation.target, version);
    return animation;
}

struct AnimationTableHeader
{
    std::uint16_t count;
    std::uint16_t countAtStart;
    std::vector<std::uint32_t> sizes;
};

AnimationTableHeader ReadAnimationTableHeader(ByteReader& reader, std::size_t table)
{
    reader.MoveTo(table);
    AnimationTableHeader header;
    header.count = reader.ReadUint16();
    header.countAtStart = reader.ReadUint16();
    reader.Skip(header.count * sizeof(std::uint32_t));
    for (std::uint16_t number = 0; number < header.count; ++number)
        header.sizes.push_back(reader.ReadUint32());
    return header;
}

std::size_t TableSize(const AnimationTableHeader& header)
{
    return TableHeaderSize + header.count * 2 * sizeof(std::uint32_t);
}

void ReadAnimations(ByteReader& reader, std::size_t particleTable, std::size_t blockEnd, EmitterResource& resource)
{
    AnimationTableHeader particleHeader = ReadAnimationTableHeader(reader, particleTable);
    std::size_t emitterTable = particleTable + TableSize(particleHeader);
    AnimationTableHeader emitterHeader = ReadAnimationTableHeader(reader, emitterTable);

    std::size_t next = emitterTable + TableSize(emitterHeader);
    for (std::uint32_t size : particleHeader.sizes)
    {
        resource.particleAnimations.push_back(ReadAnimation(reader, next, resource.version, false));
        next += size;
    }
    for (std::uint32_t size : emitterHeader.sizes)
    {
        resource.emitterAnimations.push_back(ReadAnimation(reader, next, resource.version, true));
        next += size;
    }
    if (next != blockEnd)
        reader.Fail(std::format("the emitter {} ends at {:#x} instead of {:#x}", resource.name, next, blockEnd));

    resource.particleAnimationsAtBirth = particleHeader.countAtStart;
    resource.emitterAnimationsAtStart = emitterHeader.countAtStart;
}

EmitterResource ReadEmitter(ByteReader& reader, const TableEntry& entry, std::size_t tableOffset, std::uint16_t version)
{
    std::size_t block = tableOffset + entry.offset;
    reader.MoveTo(block + sizeof(std::uint32_t));
    std::uint32_t descriptionSize = reader.ReadUint32();

    EmitterResource resource;
    resource.name = entry.name;
    resource.version = version;
    resource.emitter = ReadEmitterSettings(reader, block);

    if (version == EffectFileVersion5)
    {
        resource.drawSetting = ReadDrawSettingVersion5(reader, block + Version5DrawSettingPlace);
        resource.shape = ReadShapeVersion5(reader, block, resource.emitter.commonFlags);
    }
    else
    {
        resource.drawSetting = ReadDrawSettingVersion11(reader, block + Version11DrawSettingPlace);
        resource.shape = ReadShapeVersion11(reader, block + Version11DrawSettingPlace, resource.drawSetting.flags);
    }

    std::size_t parametersSizePlace = block + EmitterDescriptionPlace + descriptionSize;
    reader.MoveTo(parametersSizePlace);
    std::uint32_t parametersSize = reader.ReadUint32();
    std::size_t parameters = parametersSizePlace + sizeof(std::uint32_t);
    resource.particle = ReadParticleParameters(reader, parameters, version);

    ReadAnimations(reader, parameters + parametersSize, block + entry.size, resource);
    return resource;
}

TextureFormat ReadTextureFormat(ByteReader& reader)
{
    std::uint8_t number = reader.ReadByte();
    std::optional<TextureFormat> format = TextureFormatFromNumber(number);
    if (!format)
        reader.Fail(std::format("the texture format {} is not known", number));
    return *format;
}

}

const EmitterResource* EffectFile::FindEmitter(std::string_view name) const
{
    for (const EmitterResource& emitter : emitters)
    {
        if (emitter.name == name)
            return &emitter;
    }
    return nullptr;
}

EffectFile ReadEffectFile(ByteView bytes)
{
    ByteReader reader(bytes, "effect file");

    EffectFile file;
    std::size_t tableOffset = CheckFrame(reader, EffectFileTag, file.version);
    reader.MoveTo(ProjectNameLengthPlace);
    std::uint16_t nameLength = reader.ReadUint16();
    reader.MoveTo(ProjectNamePlace);
    file.projectName = reader.ReadText(nameLength);

    for (const TableEntry& entry : ReadEntryTable(reader, tableOffset))
        file.emitters.push_back(ReadEmitter(reader, entry, tableOffset, file.version));
    return file;
}

std::vector<EffectTexture> ReadEffectTextureFile(ByteView bytes)
{
    ByteReader reader(bytes, "effect texture file");

    std::uint16_t version = 0;
    std::size_t tableOffset = CheckFrame(reader, EffectTextureFileTag, version);

    std::vector<EffectTexture> textures;
    for (const TableEntry& entry : ReadEntryTable(reader, tableOffset))
    {
        std::size_t block = tableOffset + entry.offset;
        reader.MoveTo(block + sizeof(std::uint32_t));

        EffectTexture texture;
        texture.name = entry.name;
        texture.width = reader.ReadUint16();
        texture.height = reader.ReadUint16();
        std::uint32_t imageSize = reader.ReadUint32();
        reader.MoveTo(block + TextureFormatPlace);
        TextureFormat format = ReadTextureFormat(reader);
        if (IsPaletteFormat(format))
            reader.Fail(std::format("the texture {} has a palette, which no effect file uses", entry.name));

        std::size_t headerSize = version == EffectFileVersion5 ? Version5TextureHeaderSize : Version11TextureHeaderSize;
        reader.MoveTo(block + headerSize);
        ByteView pixels = reader.ReadBytes(imageSize);
        texture.image = DecodeTexture(format, texture.width, texture.height, pixels, std::nullopt);
        textures.push_back(std::move(texture));
    }
    return textures;
}

}
