#include "Channels/MiiChannel/MiiEditor.h"

#include "Channels/MiiChannel/MiiChannelData.h"

#include "Mii/DefaultMiis.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>

namespace Channels
{

namespace
{

// The grid orders of the editor, place by place (the layout's frmIconHair_00 shows hair type 33, and so on).
constexpr std::array<int, Mii::HairTypeCount> HairGridOrder = {33, 40, 51, 44, 39, 70, 45, 49, 59, 56, 68, 31, 32, 47, 37, 48, 66, 52,
    58, 50, 55, 64, 60, 62, 43, 38, 42, 23, 67, 54, 36, 41, 65, 30, 57, 34, 12, 13, 69, 26, 4, 25, 1, 19, 5, 8, 27, 7, 14, 3, 22, 10, 6,
    20, 11, 63, 17, 35, 21, 0, 61, 16, 46, 9, 18, 2, 28, 53, 71, 24, 15, 29};

constexpr std::array<int, Mii::EyeTypeCount> EyeGridOrder = {2, 4, 0, 8, 39, 17, 1, 26, 16, 15, 27, 20, 33, 11, 19, 32, 9, 12, 23, 34,
    21, 25, 40, 35, 5, 41, 13, 36, 37, 6, 24, 30, 31, 18, 28, 46, 7, 44, 38, 42, 45, 29, 3, 43, 22, 10, 14, 47};

constexpr std::array<int, Mii::EyebrowTypeCount> EyebrowGridOrder = {6, 0, 12, 1, 9, 19, 7, 21, 8, 17, 5, 4, 11, 10, 2, 3, 14, 20,
    15, 13, 22, 18, 16, 23};

constexpr std::array<int, Mii::NoseTypeCount> NoseGridOrder = {1, 10, 2, 3, 6, 0, 5, 4, 8, 9, 7, 11};

constexpr std::array<int, Mii::MouthTypeCount> MouthGridOrder = {23, 1, 19, 21, 22, 5, 0, 8, 10, 16, 6, 13, 7, 9, 2, 17, 3, 4, 15, 11,
    20, 18, 14, 12};

// A mole is a part with two types: none and one.
constexpr int MoleTypeCount = 2;
constexpr int HiddenMoleType = 0;
constexpr int ShownMoleType = 1;

// February always has 29 days, as the face library's check of a birthday says (docs/research/Mii.md 1.2).
constexpr std::array<int, Mii::MonthCount> DaysInMonth = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// The default Miis that "Start from scratch" begins with.
constexpr std::size_t ScratchManNumber = 2;
constexpr std::size_t ScratchWomanNumber = 5;
constexpr int LightestSkin = 0;
constexpr int RedFavoriteColor = 0;

std::span<const int> GridOrderOf(EditorPart part)
{
    switch (part)
    {
    case EditorPart::Hair:
        return HairGridOrder;
    case EditorPart::Eyes:
        return EyeGridOrder;
    case EditorPart::Eyebrows:
        return EyebrowGridOrder;
    case EditorPart::Nose:
        return NoseGridOrder;
    case EditorPart::Mouth:
        return MouthGridOrder;
    default:
        return {};
    }
}

// One number of the Mii that a button changes, with its range.
struct AdjustableValue
{
    int* value;
    int lowest;
    int count;
};

// The numbers of a part that its buttons change; a part without a button of a kind leaves that number empty.
struct AdjustableValues
{
    AdjustableValue size = {};
    AdjustableValue rotation = {};
    AdjustableValue spacing = {};
    AdjustableValue vertical = {};
    AdjustableValue horizontal = {};
};

// Nothing for a part that has no buttons, or a mole that is not shown.
std::optional<AdjustableValues> AdjustableValuesOf(Mii::MiiRecord& record, EditorPart part)
{
    AdjustableValues values;
    switch (part)
    {
    case EditorPart::Eyebrows:
        values.size = {&record.eyebrows.size, 0, Mii::EyebrowSizeCount};
        values.rotation = {&record.eyebrows.rotation, 0, Mii::EyebrowRotationCount};
        values.spacing = {&record.eyebrows.spacing, 0, Mii::EyebrowSpacingCount};
        values.vertical = {&record.eyebrows.vertical, Mii::EyebrowLowestVertical, Mii::EyebrowVerticalCount - Mii::EyebrowLowestVertical};
        return values;
    case EditorPart::Eyes:
        values.size = {&record.eyes.size, 0, Mii::EyeSizeCount};
        values.rotation = {&record.eyes.rotation, 0, Mii::EyeRotationCount};
        values.spacing = {&record.eyes.spacing, 0, Mii::EyeSpacingCount};
        values.vertical = {&record.eyes.vertical, 0, Mii::EyeVerticalCount};
        return values;
    case EditorPart::Nose:
        values.size = {&record.nose.size, 0, Mii::NoseSizeCount};
        values.vertical = {&record.nose.vertical, 0, Mii::NoseVerticalCount};
        return values;
    case EditorPart::Mouth:
        values.size = {&record.mouth.size, 0, Mii::MouthSizeCount};
        values.vertical = {&record.mouth.vertical, 0, Mii::MouthVerticalCount};
        return values;
    case EditorPart::Glasses:
        values.size = {&record.glasses.size, 0, Mii::GlassesSizeCount};
        values.vertical = {&record.glasses.vertical, 0, Mii::GlassesVerticalCount};
        return values;
    case EditorPart::Mustache:
        values.size = {&record.facialHair.mustacheSize, 0, Mii::MustacheSizeCount};
        values.vertical = {&record.facialHair.mustacheVertical, 0, Mii::MustacheVerticalCount};
        return values;
    case EditorPart::Mole:
        if (!record.mole.isVisible)
            return std::nullopt;
        values.size = {&record.mole.size, 0, Mii::MoleSizeCount};
        values.vertical = {&record.mole.vertical, 0, Mii::MoleVerticalCount};
        values.horizontal = {&record.mole.horizontal, 0, Mii::MoleHorizontalCount};
        return values;
    default:
        return std::nullopt;
    }
}

AdjustResult Step(AdjustableValue adjustable, int step)
{
    if (adjustable.value == nullptr)
        return AdjustResult::NotAdjustable;

    int newValue = *adjustable.value + step;
    if (newValue < adjustable.lowest || newValue >= adjustable.lowest + adjustable.count)
        return AdjustResult::AtLimit;

    *adjustable.value = newValue;
    return AdjustResult::Changed;
}

}

int TypeCount(EditorPart part)
{
    switch (part)
    {
    case EditorPart::FaceShape:
        return Mii::FaceShapeCount;
    case EditorPart::MakeUp:
        return Mii::FacialFeatureCount;
    case EditorPart::Hair:
        return Mii::HairTypeCount;
    case EditorPart::Eyebrows:
        return Mii::EyebrowTypeCount;
    case EditorPart::Eyes:
        return Mii::EyeTypeCount;
    case EditorPart::Nose:
        return Mii::NoseTypeCount;
    case EditorPart::Mouth:
        return Mii::MouthTypeCount;
    case EditorPart::Glasses:
        return Mii::GlassesTypeCount;
    case EditorPart::Mustache:
        return Mii::MustacheTypeCount;
    case EditorPart::Beard:
        return Mii::BeardTypeCount;
    case EditorPart::Mole:
        return MoleTypeCount;
    }
    return 0;
}

int ColorCount(ColorPart part)
{
    switch (part)
    {
    case ColorPart::Skin:
        return Mii::SkinColorCount;
    case ColorPart::Hair:
        return Mii::HairColorCount;
    case ColorPart::Eyebrows:
        return Mii::EyebrowColorCount;
    case ColorPart::Eyes:
        return Mii::EyeColorCount;
    case ColorPart::Mouth:
        return Mii::MouthColorCount;
    case ColorPart::Glasses:
        return Mii::GlassesColorCount;
    case ColorPart::FacialHair:
        return Mii::FacialHairColorCount;
    case ColorPart::Favorite:
        return Mii::FavoriteColorCount;
    }
    return 0;
}

int TypeAtGridPlace(EditorPart part, int place)
{
    std::span<const int> order = GridOrderOf(part);
    if (order.empty())
        return place;

    return order[place];
}

int GridPlaceOfType(EditorPart part, int type)
{
    std::span<const int> order = GridOrderOf(part);
    if (order.empty())
        return type;

    auto found = std::find(order.begin(), order.end(), type);
    return static_cast<int>(found - order.begin());
}

Mii::MiiRecord MakeScratchMii(Mii::Gender gender)
{
    Mii::MiiRecord record;
    if (gender == Mii::Gender::Female)
    {
        record = Mii::DefaultMii(ScratchWomanNumber);
    }
    else
    {
        record = Mii::DefaultMii(ScratchManNumber);
        record.face.skinColor = LightestSkin;
    }

    record.favoriteColor = RedFavoriteColor;
    record.name.clear();
    record.creatorName.clear();
    record.miiId = 0;
    record.systemId = 0;
    record.mayNotTravel = true;
    return record;
}

MiiEditor::MiiEditor(const Mii::MiiRecord& editedRecord)
    : startRecord(editedRecord)
    , record(editedRecord)
{
}

const Mii::MiiRecord& MiiEditor::Record() const
{
    return record;
}

bool MiiEditor::HasChanged() const
{
    return StoredBytes(record) != StoredBytes(startRecord);
}

int MiiEditor::ChosenType(EditorPart part) const
{
    switch (part)
    {
    case EditorPart::FaceShape:
        return record.face.shape;
    case EditorPart::MakeUp:
        return record.face.facialFeature;
    case EditorPart::Hair:
        return record.hair.type;
    case EditorPart::Eyebrows:
        return record.eyebrows.type;
    case EditorPart::Eyes:
        return record.eyes.type;
    case EditorPart::Nose:
        return record.nose.type;
    case EditorPart::Mouth:
        return record.mouth.type;
    case EditorPart::Glasses:
        return record.glasses.type;
    case EditorPart::Mustache:
        return record.facialHair.mustacheType;
    case EditorPart::Beard:
        return record.facialHair.beardType;
    case EditorPart::Mole:
        return record.mole.isVisible ? ShownMoleType : HiddenMoleType;
    }
    return 0;
}

void MiiEditor::ChooseType(EditorPart part, int type)
{
    if (type < 0 || type >= TypeCount(part))
        return;

    switch (part)
    {
    case EditorPart::FaceShape:
        record.face.shape = type;
        return;
    case EditorPart::MakeUp:
        record.face.facialFeature = type;
        return;
    case EditorPart::Hair:
        record.hair.type = type;
        return;
    case EditorPart::Eyebrows:
        record.eyebrows.type = type;
        return;
    case EditorPart::Eyes:
        record.eyes.type = type;
        return;
    case EditorPart::Nose:
        record.nose.type = type;
        return;
    case EditorPart::Mouth:
        record.mouth.type = type;
        return;
    case EditorPart::Glasses:
        record.glasses.type = type;
        return;
    case EditorPart::Mustache:
        record.facialHair.mustacheType = type;
        return;
    case EditorPart::Beard:
        record.facialHair.beardType = type;
        return;
    case EditorPart::Mole:
        record.mole.isVisible = type == ShownMoleType;
        return;
    }
}

int MiiEditor::ChosenColor(ColorPart part) const
{
    switch (part)
    {
    case ColorPart::Skin:
        return record.face.skinColor;
    case ColorPart::Hair:
        return record.hair.color;
    case ColorPart::Eyebrows:
        return record.eyebrows.color;
    case ColorPart::Eyes:
        return record.eyes.color;
    case ColorPart::Mouth:
        return record.mouth.color;
    case ColorPart::Glasses:
        return record.glasses.color;
    case ColorPart::FacialHair:
        return record.facialHair.color;
    case ColorPart::Favorite:
        return record.favoriteColor;
    }
    return 0;
}

void MiiEditor::ChooseColor(ColorPart part, int color)
{
    if (color < 0 || color >= ColorCount(part))
        return;

    switch (part)
    {
    case ColorPart::Skin:
        record.face.skinColor = color;
        return;
    case ColorPart::Hair:
        record.hair.color = color;
        return;
    case ColorPart::Eyebrows:
        record.eyebrows.color = color;
        return;
    case ColorPart::Eyes:
        record.eyes.color = color;
        return;
    case ColorPart::Mouth:
        record.mouth.color = color;
        return;
    case ColorPart::Glasses:
        record.glasses.color = color;
        return;
    case ColorPart::FacialHair:
        record.facialHair.color = color;
        return;
    case ColorPart::Favorite:
        record.favoriteColor = color;
        return;
    }
}

// Larger and smaller change the size; the turns change the rotation (turning left counts up, Assumed: CHN-65); closer and farther the
// spacing; up and down the vertical place, which grows downward on the face; left and right the place of a mole across the face.
AdjustResult MiiEditor::Adjust(EditorPart part, Adjustment adjustment)
{
    std::optional<AdjustableValues> values = AdjustableValuesOf(record, part);
    if (!values)
        return AdjustResult::NotAdjustable;

    switch (adjustment)
    {
    case Adjustment::Larger:
        return Step(values->size, 1);
    case Adjustment::Smaller:
        return Step(values->size, -1);
    case Adjustment::TurnLeft:
        return Step(values->rotation, 1);
    case Adjustment::TurnRight:
        return Step(values->rotation, -1);
    case Adjustment::Closer:
        return Step(values->spacing, -1);
    case Adjustment::Farther:
        return Step(values->spacing, 1);
    case Adjustment::Up:
        return Step(values->vertical, -1);
    case Adjustment::Down:
        return Step(values->vertical, 1);
    case Adjustment::Left:
        return Step(values->horizontal, -1);
    case Adjustment::Right:
        return Step(values->horizontal, 1);
    }
    return AdjustResult::NotAdjustable;
}

void MiiEditor::FlipHair()
{
    record.hair.isMirrored = !record.hair.isMirrored;
}

void MiiEditor::SetHeight(int height)
{
    record.height = std::clamp(height, 0, Mii::MaximumHeight);
}

void MiiEditor::SetWeight(int weight)
{
    record.weight = std::clamp(weight, 0, Mii::MaximumWeight);
}

void MiiEditor::SetNickname(const std::u16string& nickname)
{
    record.name = nickname.substr(0, Mii::MiiNameLength);
}

void MiiEditor::SetCreatorName(const std::u16string& creatorName)
{
    record.creatorName = creatorName.substr(0, Mii::MiiNameLength);
}

void MiiEditor::SetGender(Mii::Gender gender)
{
    record.gender = gender;
}

void MiiEditor::SetBirthday(int month, int day)
{
    if (month <= 0 || month > Mii::MonthCount)
    {
        record.birthMonth = 0;
        record.birthDay = 0;
        return;
    }

    record.birthMonth = month;
    record.birthDay = std::clamp(day, 0, DaysInMonth[month - 1]);
}

void MiiEditor::SetFavorite(bool isFavorite)
{
    record.isFavorite = isFavorite;
}

void MiiEditor::SetMingles(bool mingles)
{
    if (Mii::IsSpecialMii(record))
        return;

    record.mayNotTravel = !mingles;
}

}
