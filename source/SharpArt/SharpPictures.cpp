#include "SharpArt/SharpPictures.h"

#include "SharpArt/ClockPictures.h"
#include "SharpArt/CursorPictures.h"
#include "SharpArt/DataManagementPictures.h"
#include "SharpArt/DialogPictures.h"
#include "SharpArt/HomeMenuPictures.h"
#include "SharpArt/RemotePictures.h"
#include "SharpArt/SettingsPictures.h"
#include "SharpArt/WiiLogoPictures.h"
#include "SharpArt/WiiMenuPictures.h"
#include "SharpArt/WiiOptionsPictures.h"

#include "Formats/Crc32.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <mutex>

namespace SharpArt
{

namespace
{

using enum PictureChannels;

// The clock shows its 48 by 48 digits on panes of 88 by 64 units.
constexpr float ClockMagnification = 2;
constexpr float ShownAtItsSize = 1;

// The hand's pane is 54 units wide for its 64 pixels, and the pointer's layout is stretched 1.37 times to the width of the screen.
constexpr float HandMagnification = 1.2f;

// The steps of the volume grow up to almost three times their height.
constexpr float VolumeStepMagnification = 3;
constexpr Color TransparentWhite = {255, 255, 255, 0};
constexpr Color TransparentArrowBlue = {0, 34, 136, 0};
constexpr Color TransparentButtonGray = {193, 193, 193, 0};
constexpr Color TransparentCardBlue = {51, 187, 238, 0};
constexpr Color TransparentBlack = {0, 0, 0, 0};
constexpr Color TransparentDiscGray = {160, 160, 160, 0};
constexpr Color TransparentNavy = {0, 17, 85, 0};
constexpr Color TransparentButtonGrayOfHome = {102, 102, 102, 0};

constexpr std::array AllPictures = {
    SharpPicture{"my_Clock_a0.tpl", 48, 48, 0xFD378172, ClockMagnification, Intensity, DrawClockDigitZero},
    SharpPicture{"my_Clock_a1.tpl", 48, 48, 0xC4473943, ClockMagnification, Intensity, DrawClockDigitOne},
    SharpPicture{"my_Clock_a2.tpl", 48, 48, 0xEF1E74F5, ClockMagnification, Intensity, DrawClockDigitTwo},
    SharpPicture{"my_Clock_a3.tpl", 48, 48, 0x36F2C3CE, ClockMagnification, Intensity, DrawClockDigitThree},
    SharpPicture{"my_Clock_a4.tpl", 48, 48, 0x5298AB2A, ClockMagnification, Intensity, DrawClockDigitFour},
    SharpPicture{"my_Clock_a5.tpl", 48, 48, 0x7A1CB524, ClockMagnification, Intensity, DrawClockDigitFive},
    SharpPicture{"my_Clock_a6.tpl", 48, 48, 0x3B2946B9, ClockMagnification, Intensity, DrawClockDigitSix},
    SharpPicture{"my_Clock_a7.tpl", 48, 48, 0x3E42D6A7, ClockMagnification, Intensity, DrawClockDigitSeven},
    SharpPicture{"my_Clock_a8.tpl", 48, 48, 0x964794FB, ClockMagnification, Intensity, DrawClockDigitEight},
    SharpPicture{"my_Clock_a9.tpl", 48, 48, 0x7DABE0B4, ClockMagnification, Intensity, DrawClockDigitNine},
    SharpPicture{"my_Clock_ab.tpl", 48, 48, 0x6A0C754B, ClockMagnification, Intensity, DrawClockColon},
    SharpPicture{"my_Clock_b0.tpl", 48, 32, 0xD4773177, ClockMagnification, Intensity, DrawClockAm},
    SharpPicture{"my_Clock_b1.tpl", 48, 32, 0xB538DE0E, ClockMagnification, Intensity, DrawClockPm},
    SharpPicture{"it_Setting_a.tpl", 256, 200, 0x55B890A4, ShownAtItsSize, ColorAndAlpha, DrawWiiSettingsButton, TransparentWhite},
    SharpPicture{"it_DataManage_a.tpl", 256, 200, 0x7B3DBB26, ShownAtItsSize, ColorAndAlpha, DrawDataManagementButton, TransparentWhite},
    SharpPicture{"it_Savedata_a.tpl", 256, 200, 0x8D222129, ShownAtItsSize, ColorAndAlpha, DrawSaveDataButton, TransparentWhite},
    SharpPicture{"it_Channel_a.tpl", 256, 200, 0xBAC2F5F6, ShownAtItsSize, ColorAndAlpha, DrawChannelsButton, TransparentWhite},
    SharpPicture{"it_DataWii_a.tpl", 256, 200, 0x9F9AD0BE, ShownAtItsSize, ColorAndAlpha, DrawWiiSaveDataButton, TransparentWhite},
    SharpPicture{"it_BarWii_a.tpl", 72, 33, 0x9988B105, ShownAtItsSize, ColorAndAlpha, DrawWiiLogoOfTheSettings, TransparentWhite},
    SharpPicture{"my_TopBtn_a0.tpl", 48, 32, 0x4B6F0A3E, ShownAtItsSize, Intensity, DrawEnvelope},
    SharpPicture{"my_TopBtn_a.tpl", 80, 80, 0xD054B06E, ShownAtItsSize, ColorAndAlpha, DrawRoundButton, TransparentButtonGray},
    SharpPicture{"my_TopBtn_c.tpl", 80, 80, 0x1ED4FB29, ShownAtItsSize, ColorAndAlpha, DrawWiiButton, TransparentButtonGray},
    SharpPicture{"my_TopBtn_h.tpl", 80, 80, 0x4A42E90D, ShownAtItsSize, Intensity, DrawButtonGlowRing},
    SharpPicture{"mn_Top_SdCard_b.tpl", 38, 48, 0x88BBE31B, ShownAtItsSize, ColorAndAlpha, DrawSdCard, TransparentCardBlue},
    SharpPicture{"my_DiskChIcon_a.tpl", 96, 96, 0x6A586F24, ShownAtItsSize, ColorAndAlpha, DrawDiscChannelDisc, TransparentDiscGray},
    SharpPicture{"my_arw_a.tpl", 64, 64, 0x15ECDE96, ShownAtItsSize, ColorAndAlpha, DrawArrowRight, TransparentArrowBlue},
    SharpPicture{"my_arw_b.tpl", 64, 64, 0x2BF2CD28, ShownAtItsSize, ColorAndAlpha, DrawArrowLeft, TransparentArrowBlue},
    SharpPicture{"th_btn_L_a1.tpl", 56, 112, 0xE12C2907, ShownAtItsSize, ColorAndAlpha, DrawBigButtonLeftEnd, TransparentButtonGrayOfHome},
    SharpPicture{"th_btn_L_a2.tpl", 8, 112, 0x119E3D1B, ShownAtItsSize, ColorAndAlpha, DrawBigButtonMiddle, TransparentButtonGrayOfHome},
    SharpPicture{"th_btry_icon_a.tpl", 48, 24, 0x9B34CA8F, ShownAtItsSize, Intensity, DrawBatteryCase},
    SharpPicture{"th_btry_icon_b.tpl", 16, 16, 0x8FAC465D, ShownAtItsSize, Intensity, DrawBatteryBar},
    SharpPicture{"th_mnsIcon_a.tpl", 24, 24, 0x47EEDDC2, ShownAtItsSize, Intensity, DrawMinusSign},
    SharpPicture{"th_plsIcon_a.tpl", 24, 24, 0x4C6F07C0, ShownAtItsSize, Intensity, DrawPlusSign},
    SharpPicture{"th_vol_a.tpl", 16, 16, 0xE969F9C3, VolumeStepMagnification, Intensity, DrawVolumeStep},
    SharpPicture{"th_12Btn_on_b.tpl", 24, 24, 0x42CD25AB, ShownAtItsSize, ColorAndAlpha, DrawBlinkingDisc, TransparentBlack},
    SharpPicture{"th_frm_b.tpl", 16, 16, 0x2B442FA5, ShownAtItsSize, ColorAndAlpha, DrawWindowCorner, TransparentWhite},
    SharpPicture{"th_frm_b.tpl", 16, 16, 0x1138DF97, ShownAtItsSize, ColorAndAlpha, DrawWindowCorner, TransparentWhite},
    SharpPicture{"my_ComBtn_c0.tpl", 32, 80, 0x6E41274A, ShownAtItsSize, ColorAndAlpha, DrawAnswerButtonLeftEnd, TransparentBlack},
    SharpPicture{"my_ComBtn_c2.tpl", 8, 80, 0xD81B90BE, ShownAtItsSize, ColorAndAlpha, DrawAnswerButtonMiddle, TransparentBlack},
    SharpPicture{"my_ComBtn_c1.tpl", 32, 80, 0xC0B4D0E8, ShownAtItsSize, ColorAndAlpha, DrawAnswerButtonRightEnd, TransparentBlack},
    SharpPicture{"my_ComBtn_c0.tpl", 32, 80, 0x561F5E94, ShownAtItsSize, ColorAndAlpha, DrawAnswerButtonLeftEnd, TransparentBlack},
    SharpPicture{"my_ComBtn_c2.tpl", 8, 80, 0x2E984DC9, ShownAtItsSize, ColorAndAlpha, DrawAnswerButtonMiddle, TransparentBlack},
    SharpPicture{"my_ComBtn_c1.tpl", 32, 80, 0x4E55D53E, ShownAtItsSize, ColorAndAlpha, DrawAnswerButtonRightEnd, TransparentBlack},
    SharpPicture{"my_ComBtn_c0.tpl", 32, 80, 0xB28E6CBB, ShownAtItsSize, ColorAndAlpha, DrawAnswerButtonLeftEnd, TransparentBlack},
    SharpPicture{"my_ComBtn_c2.tpl", 8, 80, 0x0638E2CF, ShownAtItsSize, ColorAndAlpha, DrawAnswerButtonMiddle, TransparentBlack},
    SharpPicture{"my_ComBtn_c1.tpl", 32, 80, 0x70F8C03A, ShownAtItsSize, ColorAndAlpha, DrawAnswerButtonRightEnd, TransparentBlack},
    SharpPicture{"my_DialogWindow_a.tpl", 16, 32, 0xEA070FC1, ShownAtItsSize, ColorAndAlpha, DrawDialogWindowTopCorner, TransparentBlack},
    SharpPicture{"my_DialogWindow_b.tpl", 16, 16, 0x4C7E7234, ShownAtItsSize, Intensity, DrawDialogWindowMiddle},
    SharpPicture{"my_DialogWindow_c.tpl", 16, 32, 0xD8EE093E, ShownAtItsSize, ColorAndAlpha, DrawDialogWindowBottomCorner, TransparentBlack},
    SharpPicture{"rv_cntrl_back_b.tpl", 96, 328, 0xF1D30CE4, ShownAtItsSize, ColorAndAlpha, DrawRemoteBody, TransparentBlack},
    SharpPicture{"rv_cntrl_btn_d.tpl", 96, 328, 0xD3CF1AF5, ShownAtItsSize, ColorAndAlpha, DrawRemoteButtons, TransparentBlack},
    SharpPicture{"rv_cntrl_home_a.tpl", 16, 16, 0x5DCAF0A4, ShownAtItsSize, Intensity, DrawRemoteHomeSign},
    SharpPicture{"rv_cntrl_pwr_a.tpl", 16, 16, 0x37588C3A, ShownAtItsSize, Intensity, DrawRemotePowerSign},
    SharpPicture{"rv_cntrl_sound_b.tpl", 16, 32, 0x6DF5049C, ShownAtItsSize, Intensity, DrawRemoteSpeakerLit},
    SharpPicture{"th_homeBtnImg_a.tpl", 56, 56, 0x0E7D0492, ShownAtItsSize, ColorAndAlpha, DrawHomeButtonPicture, TransparentNavy},
    SharpPicture{"th_optnBtn_a1.tpl", 24, 64, 0xCFEB5B83, ShownAtItsSize, ColorAndAlpha, DrawSettingsButtonLeftEnd, TransparentBlack},
    SharpPicture{"th_optnBtn_a2.tpl", 8, 64, 0x684A418C, ShownAtItsSize, ColorAndAlpha, DrawSettingsButtonMiddle, TransparentBlack},
    SharpPicture{"th_optnBtn_a3.tpl", 24, 64, 0x01422CAE, ShownAtItsSize, ColorAndAlpha, DrawSettingsButtonRightEnd, TransparentBlack},
    SharpPicture{"th_btn_L_a3.tpl", 56, 112, 0x74E6D175, ShownAtItsSize, ColorAndAlpha, DrawBigButtonRightEnd, TransparentButtonGrayOfHome},
    SharpPicture{"SetBtn_a0.tpl", 32, 64, 0xB33DE186, ShownAtItsSize, ColorAndAlpha, DrawSettingsPageButtonLeftEnd, TransparentWhite},
    SharpPicture{"SetBtn_a1.tpl", 8, 64, 0x0ACC8A22, ShownAtItsSize, ColorAndAlpha, DrawSettingsPageButtonMiddle, TransparentWhite},
    SharpPicture{"SetBtn_a2.tpl", 32, 64, 0xA68F5DE2, ShownAtItsSize, ColorAndAlpha, DrawSettingsPageButtonRightEnd, TransparentWhite},
    SharpPicture{"SetBtn_Focus_a0.tpl", 32, 64, 0xDBBFA318, ShownAtItsSize, Intensity, DrawSettingsPageButtonLightLeftEnd},
    SharpPicture{"SetBtn_Focus_a1.tpl", 8, 64, 0x9DE70E1B, ShownAtItsSize, Intensity, DrawSettingsPageButtonLightMiddle},
    SharpPicture{"SetBtn_Focus_a2.tpl", 32, 64, 0x59865F1F, ShownAtItsSize, Intensity, DrawSettingsPageButtonLightRightEnd},
    SharpPicture{"SetBtn_Focus_a0.tpl", 32, 64, 0xEDD9167C, ShownAtItsSize, ColorAndAlpha, DrawSettingsPageButtonLightOnBlackLeftEnd},
    SharpPicture{"SetBtn_Focus_a1.tpl", 8, 64, 0x4B0C5D91, ShownAtItsSize, ColorAndAlpha, DrawSettingsPageButtonLightOnBlackMiddle},
    SharpPicture{"SetBtn_Focus_a2.tpl", 32, 64, 0xBD6D9C15, ShownAtItsSize, ColorAndAlpha, DrawSettingsPageButtonLightOnBlackRightEnd},
    SharpPicture{"my_ArwSetUp_a.tpl", 72, 72, 0x59DC6570, ShownAtItsSize, ColorAndAlpha, DrawSettingsPageArrowUp},
    SharpPicture{"my_ArwSetUp_b.tpl", 72, 72, 0x88858AC2, ShownAtItsSize, ColorAndAlpha, DrawSettingsPageArrowDown},
    SharpPicture{"fs_ArwSetUp_a.tpl", 72, 52, 0x45123048, ShownAtItsSize, ColorAndAlpha, DrawKeyboardArrowUp},
    SharpPicture{"fs_ArwSetUp_b.tpl", 72, 52, 0x5072185D, ShownAtItsSize, ColorAndAlpha, DrawKeyboardArrowDown},
    SharpPicture{"it_Button_a.tpl", 144, 112, 0x485E520B, ShownAtItsSize, ColorAndAlpha, DrawBigSettingsButton, TransparentWhite},
    SharpPicture{"defcursor_final64_a.tpl", 64, 64, 0x19BCCC5B, HandMagnification, ColorAndAlpha, DrawPointingHand},
    SharpPicture{"defcursor_final64_b.tpl", 64, 64, 0x8AC8ECB8, HandMagnification, ColorAndAlpha, DrawGrabbingHand},
    SharpPicture{"defcursor_final64_c.tpl", 64, 64, 0x226F18A6, HandMagnification, ColorAndAlpha, DrawOpenHand},
    SharpPicture{"defcursor_final_p0.tpl", 32, 32, 0x432C1665, HandMagnification, Intensity, DrawHandShadeWithoutNumber},
    SharpPicture{"defcursor_final_p1.tpl", 64, 64, 0xCF0560B3, HandMagnification, Intensity, DrawHandShadeOfPlayerOne},
    SharpPicture{"defcursor_final_p2.tpl", 64, 64, 0xCFD81A13, HandMagnification, Intensity, DrawHandShadeOfPlayerTwo},
    SharpPicture{"defcursor_final_p3.tpl", 64, 64, 0xC137D4C3, HandMagnification, Intensity, DrawHandShadeOfPlayerThree},
    SharpPicture{"defcursor_final_p4.tpl", 64, 64, 0x594936FB, HandMagnification, Intensity, DrawHandShadeOfPlayerFour},
};

constexpr float FullByte = 255;
constexpr std::string_view WordPicturePrefix = "tx_";

bool IsIntensityPixel(const Formats::Rgba8Pixel& pixel)
{
    return pixel.red == pixel.alpha && pixel.green == pixel.alpha && pixel.blue == pixel.alpha;
}

void TurnIntoIntensity(Formats::RgbaImage& picture)
{
    for (Formats::Rgba8Pixel& pixel : picture.pixels)
    {
        auto intensity = static_cast<Formats::Byte>(std::lround(pixel.red * (pixel.alpha / FullByte)));
        pixel = {intensity, intensity, intensity, intensity};
    }
}

}

std::span<const SharpPicture> AllSharpPictures()
{
    return AllPictures;
}

bool HasSharpPicture(std::string_view textureFileName)
{
    for (const SharpPicture& picture : AllPictures)
    {
        if (picture.textureFileName == textureFileName)
            return true;
    }
    return false;
}

const SharpPicture* FindSharpPicture(std::string_view textureFileName, const Formats::RgbaImage& original)
{
    for (const SharpPicture& picture : AllPictures)
    {
        if (picture.textureFileName == textureFileName && IsTheOriginalOf(picture, original))
            return &picture;
    }
    return nullptr;
}

std::uint32_t FingerprintOf(const Formats::RgbaImage& picture)
{
    Formats::Bytes bytes;
    bytes.reserve(picture.pixels.size() * 4);
    for (const Formats::Rgba8Pixel& pixel : picture.pixels)
    {
        bytes.push_back(pixel.red);
        bytes.push_back(pixel.green);
        bytes.push_back(pixel.blue);
        bytes.push_back(pixel.alpha);
    }
    return Formats::ComputeCrc32(bytes);
}

bool IsTheOriginalOf(const SharpPicture& sharpPicture, const Formats::RgbaImage& picture)
{
    if (picture.width != sharpPicture.width || picture.height != sharpPicture.height)
        return false;

    return FingerprintOf(picture) == sharpPicture.originalFingerprint;
}

bool IsPictureOfWords(std::string_view textureFileName, const Formats::RgbaImage& picture)
{
    if (!textureFileName.starts_with(WordPicturePrefix))
        return false;

    return std::ranges::all_of(picture.pixels, IsIntensityPixel);
}

Formats::RgbaImage DrawSharpPicture(const SharpPicture& sharpPicture, std::uint32_t pixelsPerUnit)
{
    VectorCanvas canvas(sharpPicture.width, sharpPicture.height, pixelsPerUnit, sharpPicture.transparentColor);
    sharpPicture.draw(canvas);

    Formats::RgbaImage picture = std::move(canvas.Picture());
    if (sharpPicture.channels == Intensity)
        TurnIntoIntensity(picture);
    return picture;
}

namespace
{

// The latest drawing of each sharp picture and its pixels per unit.
struct KeptDrawing
{
    std::uint32_t pixelsPerUnit = 0;
    std::shared_ptr<const Formats::RgbaImage> picture;
};

std::mutex keptDrawingsLock;
std::map<const SharpPicture*, KeptDrawing> keptDrawings;

}

std::shared_ptr<const Formats::RgbaImage> KeptSharpPicture(const SharpPicture& sharpPicture, std::uint32_t pixelsPerUnit)
{
    std::lock_guard lock(keptDrawingsLock);
    KeptDrawing& kept = keptDrawings[&sharpPicture];
    if (kept.picture && kept.pixelsPerUnit == pixelsPerUnit)
        return kept.picture;

    kept.picture = std::make_shared<const Formats::RgbaImage>(DrawSharpPicture(sharpPicture, pixelsPerUnit));
    kept.pixelsPerUnit = pixelsPerUnit;
    return kept.picture;
}

std::uint32_t PixelsPerUnitFor(const SharpPicture& sharpPicture, float screenPixelsPerUnit)
{
    float needed = std::ceil(screenPixelsPerUnit * sharpPicture.largestMagnification);
    return static_cast<std::uint32_t>(std::clamp(needed, 1.0f, static_cast<float>(MostPixelsPerUnit)));
}

}
