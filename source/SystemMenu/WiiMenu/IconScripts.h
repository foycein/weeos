#pragma once

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/WiiMenu/FramePlayer.h"

#include "Ios/Title/TitleId.h"
#include "Layout/LayoutArchive.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SystemMenu
{

// Four system channels bring a script for their icon (icon.cs, docs/research/Channels.md 1.6). The menu runs it when it makes the
// tile, and again every ten minutes. WeeOS runs no script: it does here, as plain code, what each script does while its channel has
// no data of its own, which is always the case in WeeOS (no WiiConnect24, nothing posted to the Wii Menu); without data a later run
// changes nothing. The scripts were read in their byte code.
enum class IconScript
{
    // No script, or the script of a channel that WeeOS does not know: the icon shows its language and plays icon_Start.
    None,
    PhotoChannel,
    ShopChannel,
    ForecastChannel,
    NewsChannel,
};

// The script of a channel's icon, by the first three letters of its title. Only a channel whose banner names a script has one.
IconScript FindIconScript(Ios::TitleId titleId, bool isIconDrivenByCode);

// An icon has at most 16 animations for its script, icon_Rso0 to icon_Rso15.
constexpr int ScriptAnimationCount = 16;

// The animations icon_Rso0 to icon_Rso15 that an icon has. Only the script starts them, so until then they change nothing.
// Each plays on the panes of the group with the same name (Rso0 to Rso15), or on the whole icon when there is no such group (0x813B281C).
class ScriptAnimations
{
public:
    // The numbers are those of the animations that the icon has. The icon and the archive must outlive the animations.
    ScriptAnimations(MenuLayout& icon, const Layout::LayoutArchive& archive, const std::vector<int>& numbers);

    // Plays from the first frame, once or as a loop as the file says (Anim.start of a script). An animation that plays once
    // stops on the last frame of its file; a loop starts again after it (0x81369ABC, 0x81362890).
    void Start(int number);

    // The same, but the animation ends at a frame that the script chooses (Anim.setMaxFrame and Anim.restart).
    void StartWithEnd(int number, float endFrame);

    // Shows the first frame once and stops (Anim.initFrame(0)).
    void ShowFirstFrame(int number);

    void Step();

private:
    FramePlayer* Find(int number);

    std::map<int, std::unique_ptr<FramePlayer>> players;
    std::map<int, bool> isLoopByNumber;
};

// Does what the script of the icon does when the tile is made and the channel has no data.
void RunIconScript(IconScript script, MenuLayout& icon, ScriptAnimations& animations);

// The name of the animation file of a script animation, for example "icon_Rso0".
std::string ScriptAnimationName(int number);

// The name of the group that a script animation plays on, for example "Rso0".
std::string ScriptGroupName(int number);

}
