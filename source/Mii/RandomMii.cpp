#include "Mii/RandomMii.h"

#include "Mii/MiiRotationOffsets.h"
#include "Mii/RandomMiiTables.h"

namespace Mii
{

namespace
{

constexpr int GendersPerRow = 9;
constexpr int AgesPerRow = 3;
constexpr int LooksPerRow = 3;

constexpr int StandardSize = 4;
constexpr int SmallFemaleNoseSize = 3;
constexpr int StandardEyeSpacing = 2;
constexpr int StandardEyebrowSpacing = 2;
constexpr int StandardEyeVertical = 12;
constexpr int StandardEyebrowVertical = 10;
constexpr int EastAsianEyebrowVertical = 9;
constexpr int StandardNoseVertical = 9;
constexpr int StandardMouthVertical = 13;
constexpr int StandardMustacheVertical = 10;
constexpr int StandardGlassesVertical = 10;
constexpr int StandardHeight = 64;
constexpr int StandardWeight = 64;

// Women and children have their features a little lower: 0 to 2 steps each, drawn separately.
constexpr int LowerFeatureStepCount = 3;

// Chances are drawn as a number from 0 to 9.
constexpr int ChanceCount = 10;
constexpr int CoinSideCount = 2;

// Random ages: 0 to 3 a child, 4 to 7 an adult, 8 and 9 an elder. Random looks: 0 to 3 East Asian, 4 to 7 light skin, 8 and 9 dark skin.
constexpr int FirstChanceLimit = 4;
constexpr int SecondChanceLimit = 8;

// Two in ten adult or elder men get facial hair: a beard, a mustache or both, each of type 1 to 3.
constexpr int FacialHairChanceLimit = 2;
constexpr int FacialHairStyleCount = 3;
constexpr int FacialHairTypeCount = 3;

constexpr std::u16string_view RandomMiiName = u"no name";

// The glasses of a random Mii, from a number from 0 to 99: the first entry whose limit is above it.
struct GlassesChance
{
    int limit;
    int type;
};

const std::vector<GlassesChance> ChildGlasses = {{90, 0}, {94, 1}, {96, 2}, {100, 3}};
const std::vector<GlassesChance> AdultGlasses = {{83, 0}, {86, 1}, {90, 2}, {93, 3}, {94, 4}, {96, 5}, {98, 6}, {100, 7}};
const std::vector<GlassesChance> ElderGlasses = {{78, 0}, {83, 1}, {93, 3}, {98, 6}, {100, 7}};
constexpr int PercentCount = 100;

// The original reduces a fresh reading of the processor's clock with "%" for every choice; this does the same with the generator.
int DrawBelow(std::mt19937& random, int count)
{
    return static_cast<int>(random() % static_cast<unsigned>(count));
}

template <typename Row>
int DrawFrom(std::mt19937& random, const Row& choices)
{
    return choices[DrawBelow(random, static_cast<int>(choices.size()))];
}

int ChooseGlassesType(MiiAge age, std::mt19937& random)
{
    const std::vector<GlassesChance>* chances = &ChildGlasses;
    if (age == MiiAge::Adult)
        chances = &AdultGlasses;
    if (age == MiiAge::Elder)
        chances = &ElderGlasses;

    int number = DrawBelow(random, PercentCount);
    for (const GlassesChance& chance : *chances)
    {
        if (number < chance.limit)
            return chance.type;
    }
    return 0;
}

int UprightRotation(int offset)
{
    return (RotationStepsPerTurn - offset) % RotationStepsPerTurn;
}

MiiFacialHair ChooseFacialHair(int hairColor, std::mt19937& random)
{
    MiiFacialHair facialHair;
    if (DrawBelow(random, ChanceCount) >= FacialHairChanceLimit)
        return facialHair;

    int style = DrawBelow(random, FacialHairStyleCount);
    bool hasBeard = style == 0 || style == 2;
    bool hasMustache = style == 1 || style == 2;

    if (hasBeard)
        facialHair.beardType = DrawBelow(random, FacialHairTypeCount) + 1;
    if (hasMustache)
        facialHair.mustacheType = DrawBelow(random, FacialHairTypeCount) + 1;

    facialHair.color = hairColor;
    facialHair.mustacheSize = StandardSize;
    facialHair.mustacheVertical = StandardMustacheVertical;
    return facialHair;
}

}

MiiRecord MakeRandomMii(Gender gender, MiiAge age, MiiLook look, std::mt19937& random)
{
    int genderNumber = static_cast<int>(gender);
    int ageNumber = static_cast<int>(age);
    int lookNumber = static_cast<int>(look);
    int row = genderNumber * GendersPerRow + ageNumber * AgesPerRow + lookNumber;
    bool isFemale = gender == Gender::Female;

    int lowerSteps = 0;
    if (isFemale)
        lowerSteps += DrawBelow(random, LowerFeatureStepCount);
    if (age == MiiAge::Child)
        lowerSteps += DrawBelow(random, LowerFeatureStepCount);

    MiiRecord record;
    record.gender = gender;

    record.face.shape = DrawFrom(random, FaceShapeChoices[row]);
    record.face.skinColor = DrawFrom(random, SkinColorChoices[genderNumber * LooksPerRow + lookNumber]);
    record.face.facialFeature = DrawFrom(random, FacialFeatureChoices[row]);

    record.hair.type = DrawFrom(random, HairTypeChoices[row]);
    record.hair.color = DrawFrom(random, HairColorChoices[lookNumber * AgesPerRow + ageNumber]);
    record.hair.isMirrored = DrawBelow(random, CoinSideCount) == 1;

    record.eyes.type = DrawFrom(random, EyeTypeChoices[row]);
    record.eyes.color = DrawFrom(random, EyeColorChoices[lookNumber]);
    record.eyes.size = StandardSize;
    record.eyes.spacing = StandardEyeSpacing;
    record.eyes.vertical = StandardEyeVertical + lowerSteps;
    record.eyes.rotation = UprightRotation(EyeRotationOffset[record.eyes.type]);

    record.eyebrows.type = DrawFrom(random, EyebrowTypeChoices[row]);
    record.eyebrows.color = record.hair.color;
    record.eyebrows.size = StandardSize;
    record.eyebrows.spacing = StandardEyebrowSpacing;
    record.eyebrows.rotation = UprightRotation(EyebrowRotationOffset[record.eyebrows.type]);
    record.eyebrows.vertical = StandardEyebrowVertical + lowerSteps;
    if (look == MiiLook::EastAsian)
        record.eyebrows.vertical = EastAsianEyebrowVertical + lowerSteps;

    record.nose.type = DrawFrom(random, NoseTypeChoices[row]);
    record.nose.size = StandardSize;
    if (isFemale)
        record.nose.size = SmallFemaleNoseSize;
    record.nose.vertical = StandardNoseVertical + lowerSteps;

    record.mouth.type = DrawFrom(random, MouthTypeChoices[row]);
    if (isFemale)
        record.mouth.color = DrawBelow(random, MouthColorCount);
    record.mouth.size = StandardSize;
    record.mouth.vertical = StandardMouthVertical + lowerSteps;

    if (!isFemale && age != MiiAge::Child)
        record.facialHair = ChooseFacialHair(record.hair.color, random);

    record.glasses.type = ChooseGlassesType(age, random);
    record.glasses.size = StandardSize;
    record.glasses.vertical = StandardGlassesVertical + lowerSteps;

    record.mole.size = StandardSize;
    record.mole.horizontal = DrawBelow(random, MoleHorizontalCount);
    record.mole.vertical = DrawBelow(random, MoleVerticalCount);

    record.height = StandardHeight;
    record.weight = StandardWeight;
    record.name = RandomMiiName;
    record.favoriteColor = DrawBelow(random, FavoriteColorCount);
    record.miiId = TemporaryMiiFlag;
    return record;
}

Gender ChooseRandomGender(std::mt19937& random)
{
    if (DrawBelow(random, CoinSideCount) == 0)
        return Gender::Male;

    return Gender::Female;
}

MiiAge ChooseRandomAge(std::mt19937& random)
{
    int number = DrawBelow(random, ChanceCount);
    if (number < FirstChanceLimit)
        return MiiAge::Child;
    if (number < SecondChanceLimit)
        return MiiAge::Adult;
    return MiiAge::Elder;
}

MiiLook ChooseRandomLook(std::mt19937& random)
{
    int number = DrawBelow(random, ChanceCount);
    if (number < FirstChanceLimit)
        return MiiLook::EastAsian;
    if (number < SecondChanceLimit)
        return MiiLook::LightSkin;
    return MiiLook::DarkSkin;
}

}
