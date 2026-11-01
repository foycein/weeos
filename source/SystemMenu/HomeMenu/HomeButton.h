#pragma once

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/HomeMenu/HomeButtonMenu.h"
#include "SystemMenu/HomeMenu/HomeMenuFiles.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuFiles.h"

#include "Graphics/GraphicsDevice.h"
#include "Ios/Remote/RemoteService.h"
#include "Layout/TextFonts.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace SystemMenu
{

// The volume of the speakers of the remotes as SYSCONF keeps it (BT.SPKV, 0 to 127). A console without the setting has 0x59 (INP-01).
constexpr std::uint8_t DefaultSpeakerVolumeSetting = 0x59;

// What the HOME menu needs from the rest of the console. Each one may be left empty while that part of the console is not there.
struct HomeButtonConnections
{
    // Plays a sound of the sound archive of the menu.
    HomeMenuSoundPlayer playSound;

    // The fonts of the texts.
    const Layout::TextFonts* textFonts = nullptr;

    // The user chose "Wii Menu": the menu starts again, through the front panel, as a press of RESET does.
    std::function<void()> startReset;

    // Called whenever the menu closes, with the settings of the remotes for SYSCONF: BT.SPKV and BT.MOT.
    std::function<void(std::uint8_t speakerVolumeSetting, bool isRumbleOn)> saveRemoteSettings;
};

// The HOME button of the System Menu (the glue object 0x810890BC, 0x81347B6C to 0x81348438): it opens the HOME menu when HOME is pressed
// on a remote, holds the program under it while it is open, and acts on how it ended. A screen that must not be interrupted disables it;
// HOME then shows a small picture that says so.
class HomeButton
{
public:
    // Everything given must outlive this object. Nothing is read before Start.
    HomeButton(Display& display, const MenuFiles& files, const InputManager& input, Ios::RemoteService& remotes, CursorManager& cursors,
        const Fader& fader, HomeButtonConnections connections = {});
    ~HomeButton();

    HomeButton(const HomeButton&) = delete;
    HomeButton& operator=(const HomeButton&) = delete;

    // From now on HOME opens the menu: the Wii Menu calls this when it has come up (0x813902F4), and so does the start-up router.
    // Reads the files of the HOME menu the first time.
    void Start();

    // A screen asks for HOME to do nothing for a while. While the menu is open this happens when it has closed, and the answer is false.
    bool Disable();
    void Enable();

    // While the menu is open the screens stand still and the cursor manager does not draw: the menu draws the hands.
    bool IsOpen() const;

    // The volume of the speakers as SYSCONF keeps it. The program gives it when it has read SYSCONF.
    void SetSpeakerVolumeSetting(std::uint8_t setting);

    void Update();

    // The HOME menu, or the hands of the cursor manager when it is closed.
    void Draw();

private:
    enum class IconPhase
    {
        FadingIn,
        Showing,
        FadingOut,
    };

    void CheckHomeButton();
    void OpenMenu();
    void CloseMenu(HomeMenuOutcome outcome);
    void UpdateIcon();
    void DrawIcon();
    void DrawBlack();

    Display& display;
    const MenuFiles& menuFiles;
    const InputManager& input;
    Ios::RemoteService& remotes;
    CursorManager& cursors;
    const Fader& fader;
    HomeButtonConnections connections;

    std::optional<HomeMenuFiles> files;
    std::unique_ptr<HomeButtonMenu> menu;
    std::optional<Graphics::TextureHandle> iconTexture;

    bool isStarted = false;
    bool isEnabled = true;
    bool isDisablePending = false;
    bool isOpen = false;
    std::uint8_t speakerVolumeSetting = DefaultSpeakerVolumeSetting;

    // After "Wii Menu" the picture stays black until the menu starts again (VISetBlack of the original).
    bool isPictureBlack = false;

    std::optional<IconPhase> iconPhase;
    int iconPhaseSteps = 0;
    std::uint8_t iconAlpha = 0;
};

}
