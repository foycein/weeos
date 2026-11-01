#pragma once

#include "Mii/MiiRecord.h"

#include <string>

namespace Channels
{

// The pages of the editor, in the order of its tabs at the top of the screen (the panes mainMFrmNull_00 to _08 of editNigaoe_00).
enum class EditorPage
{
    Profile,
    Body,
    Face,
    Hair,
    Eyebrows,
    Eyes,
    Nose,
    Mouth,
    Features,
};

constexpr int EditorPageCount = 9;

// The parts whose type is chosen from a grid of pictures.
enum class EditorPart
{
    FaceShape,
    MakeUp,
    Hair,
    Eyebrows,
    Eyes,
    Nose,
    Mouth,
    Glasses,
    Mustache,
    Beard,
    Mole,
};

// The parts whose color is chosen from a palette.
enum class ColorPart
{
    Skin,
    Hair,
    Eyebrows,
    Eyes,
    Mouth,
    Glasses,
    FacialHair,
    Favorite,
};

// The buttons that size, turn and move a part. Their pictures name them (sk_editEnlargement, sk_editRotL, sk_editNear, ...).
enum class Adjustment
{
    Larger,
    Smaller,
    TurnLeft,
    TurnRight,
    Closer,
    Farther,
    Up,
    Down,
    Left,
    Right,
};

enum class AdjustResult
{
    Changed,

    // The part is as big, as far, as high as it can be: nothing changed (the sound SEQ_EDIT_PARTS_UP_LIMIT or _DOWN_LIMIT).
    AtLimit,

    // The part has no such button, for example a mole that is not shown.
    NotAdjustable,
};

int TypeCount(EditorPart part);
int ColorCount(ColorPart part);

// The type that a place of the grid shows. The editor shows hair, eyebrows, eyes, nose and mouth in an order of its own, which is the
// order of the pictures in its layout and of the tables at 0x80207DF0, 0x80207E68, 0x80207E38, 0x80207E80 and 0x80207E90 of the program;
// the other parts in the order of their types.
int TypeAtGridPlace(EditorPart part, int place);
int GridPlaceOfType(EditorPart part, int type);

// "Start from scratch" (docs/research/Mii.md 3.2): default Mii 2 with the lightest skin for a man, default Mii 5 for a woman, both with
// the favorite color red, no names and no create id yet. They may not travel.
Mii::MiiRecord MakeScratchMii(Mii::Gender gender);

// The Mii that the editor works on, and every change the pages of the editor make to it. The editor keeps every value inside the range
// that the face library accepts (docs/research/Mii.md 1.1), so the Mii it gives back is always valid once it has a nickname.
class MiiEditor
{
public:
    explicit MiiEditor(const Mii::MiiRecord& record);

    const Mii::MiiRecord& Record() const;

    // True once anything differs from the Mii the editor started with.
    bool HasChanged() const;

    int ChosenType(EditorPart part) const;
    void ChooseType(EditorPart part, int type);

    int ChosenColor(ColorPart part) const;
    void ChooseColor(ColorPart part, int color);

    AdjustResult Adjust(EditorPart part, Adjustment adjustment);

    // The button with the mirror: the hair is parted on the other side.
    void FlipHair();

    // The sliders of the body page, from 0 to 128.
    void SetHeight(int height);
    void SetWeight(int weight);

    // The profile.
    void SetNickname(const std::u16string& nickname);
    void SetCreatorName(const std::u16string& creatorName);
    void SetGender(Mii::Gender gender);

    // Month 0 and day 0 is no birthday. A day that the month does not have is made the last day of the month.
    void SetBirthday(int month, int day);

    void SetFavorite(bool isFavorite);

    // Mingle: the Mii may travel to other consoles. A special Mii never mingles.
    void SetMingles(bool mingles);

private:
    Mii::MiiRecord startRecord;
    Mii::MiiRecord record;
};

}
