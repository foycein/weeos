#include "SystemMenu/HomeMenu/HomeButton.h"

#include <algorithm>

namespace SystemMenu
{

namespace
{

constexpr ButtonSet HomeButtons = {Input::RemoteButtons::Home, Input::ClassicButtons::Home};

// BT.SPKV runs from 0 to 127 and the menu shows 10 steps (0x813729C8 reads, 0x81372E9C writes).
constexpr float SettingPerStep = 12.7f;
constexpr float StepPerSetting = 0.07874016f;
constexpr float StepRoundingOffset = 0.9f;

// The picture that says that HOME does nothing now fades in for 250 ms, stays for a second and fades out for 250 ms (0x81347F98).
constexpr double IconFadeMilliseconds = 250;
constexpr double IconShowMilliseconds = 1000;
constexpr double MillisecondsPerStep = 1001.0 / 60.0;
constexpr double HighestIconAlpha = 255.9;

// Where the picture lies, in the layout units of the 4:3 picture: the corner at the top left.
// Assumed: the same place on a 16:9 screen (docs/audit/OpenPoints/SystemMenu.md SYS-61).
constexpr float IconLeft = -256;
constexpr float IconTop = 188;
constexpr float IconRight = -200;
constexpr float IconBottom = 132;

constexpr Formats::Rgba8Color Black = {0, 0, 0, 255};
constexpr Formats::Rgba8Color White = {255, 255, 255, 255};

int SpeakerStepOf(std::uint8_t setting)
{
    return static_cast<int>(StepRoundingOffset + StepPerSetting * setting);
}

std::uint8_t SpeakerSettingOf(int step)
{
    return static_cast<std::uint8_t>(SettingPerStep * static_cast<float>(step));
}

double MillisecondsOf(int steps)
{
    return steps * MillisecondsPerStep;
}

}

HomeButton::HomeButton(Display& menuDisplay, const MenuFiles& files, const InputManager& inputManager, Ios::RemoteService& remoteService,
    CursorManager& cursorManager, const Fader& screenFader, HomeButtonConnections homeConnections)
    : HomeButton(menuDisplay, [&files] { return ReadHomeMenuFiles(files); }, inputManager, remoteService, cursorManager, screenFader,
          std::move(homeConnections))
{
}

HomeButton::HomeButton(Display& programDisplay, std::function<HomeMenuFiles()> readHomeMenuFiles, const InputManager& inputManager,
    Ios::RemoteService& remoteService, CursorManager& cursorManager, const Fader& screenFader, HomeButtonConnections homeConnections)
    : display(programDisplay)
    , readFiles(std::move(readHomeMenuFiles))
    , input(inputManager)
    , remotes(remoteService)
    , cursors(cursorManager)
    , fader(screenFader)
    , connections(std::move(homeConnections))
{
}

HomeButton::~HomeButton()
{
    if (iconTexture)
        display.Device().DestroyTexture(*iconTexture);
}

void HomeButton::Start()
{
    isStarted = true;
    if (files)
        return;

    files = readFiles();
    menu = std::make_unique<HomeButtonMenu>(display, *files, input, remotes, cursors, connections.textFonts, connections.playSound);

    Graphics::TextureDescription icon;
    icon.width = files->prohibitedIcon.width;
    icon.height = files->prohibitedIcon.height;
    icon.pixels = files->prohibitedIcon.pixels;
    iconTexture = display.Device().CreateTexture(icon);
}

bool HomeButton::Disable()
{
    if (isOpen)
    {
        isDisablePending = true;
        return false;
    }

    isEnabled = false;
    return true;
}

void HomeButton::Enable()
{
    isDisablePending = false;
    isEnabled = true;
}

void HomeButton::HoldForFrontPanel()
{
    isHeldByFrontPanel = true;
    isEnabled = false;
}

bool HomeButton::IsOpen() const
{
    return isOpen;
}

float HomeButton::SoundVolume() const
{
    if (!isOpen || !menu)
        return 1.0f;

    return menu->SoundVolume();
}

void HomeButton::SetSpeakerVolumeSetting(std::uint8_t setting)
{
    speakerVolumeSetting = setting;
}

void HomeButton::Update()
{
    if (!isStarted)
        return;

    bool canOpen = !isOpen && !iconPhase && fader.GetStatus() == FaderStatus::Clear;
    if (canOpen)
        CheckHomeButton();

    if (isOpen && !isHeldByFrontPanel)
    {
        menu->Update();
        if (menu->Outcome() != HomeMenuOutcome::Open)
            CloseMenu(menu->Outcome());
    }

    UpdateIcon();
}

// The open HOME menu draws the hands itself. Otherwise the hands of the cursor manager come over the picture that says that HOME does nothing.
void HomeButton::Draw()
{
    if (isOpen)
        menu->Draw();

    if (iconPhase)
        DrawIcon();

    if (!isOpen)
        cursors.Draw();

    if (isPictureBlack)
        DrawBlack();
}

void HomeButton::CheckHomeButton()
{
    if (!input.AnyPlayerPressed(HomeButtons))
        return;

    if (isEnabled)
    {
        OpenMenu();
        return;
    }

    iconPhase = IconPhase::FadingIn;
    iconPhaseSteps = 0;
    iconAlpha = 0;
}

void HomeButton::OpenMenu()
{
    menu->Open(SpeakerStepOf(speakerVolumeSetting));
    isOpen = true;
}

// 0x81348398 and the end of 0x81347F98: a Disable that came while the menu was open takes effect, the settings of the remotes are kept,
// and a big button that was chosen starts the reset ("HBM: Start Reset").
void HomeButton::CloseMenu(HomeMenuOutcome outcome)
{
    isOpen = false;
    if (isDisablePending)
    {
        isDisablePending = false;
        isEnabled = false;
    }

    speakerVolumeSetting = SpeakerSettingOf(menu->SpeakerVolumeStep());
    if (connections.saveRemoteSettings)
        connections.saveRemoteSettings(speakerVolumeSetting, remotes.IsRumbleEnabled());

    if (outcome != HomeMenuOutcome::ButtonChosen)
        return;

    isPictureBlack = true;
    if (connections.startReset)
        connections.startReset();

    if (connections.chooseBigButton)
        connections.chooseBigButton(menu->ChosenBigButton());
}

void HomeButton::UpdateIcon()
{
    if (!iconPhase)
        return;

    ++iconPhaseSteps;
    double milliseconds = MillisecondsOf(iconPhaseSteps);

    switch (*iconPhase)
    {
    case IconPhase::FadingIn:
        iconAlpha = static_cast<std::uint8_t>(HighestIconAlpha * std::min(milliseconds / IconFadeMilliseconds, 1.0));
        if (milliseconds >= IconFadeMilliseconds)
        {
            iconPhase = IconPhase::Showing;
            iconPhaseSteps = 0;
        }
        break;

    case IconPhase::Showing:
        if (milliseconds >= IconShowMilliseconds)
        {
            iconPhase = IconPhase::FadingOut;
            iconPhaseSteps = 0;
        }
        break;

    case IconPhase::FadingOut:
        iconAlpha = static_cast<std::uint8_t>(HighestIconAlpha * std::max(1.0 - milliseconds / IconFadeMilliseconds, 0.0));
        if (milliseconds >= IconFadeMilliseconds)
            iconPhase.reset();
        break;
    }
}

void HomeButton::DrawIcon()
{
    Formats::Rgba8Color color = White;
    color.alpha = iconAlpha;

    Graphics::Quad quad = {};
    quad.positions = {{{IconLeft, IconTop}, {IconRight, IconTop}, {IconRight, IconBottom}, {IconLeft, IconBottom}}};
    quad.colors = {{color, color, color, color}};
    quad.textureCoordinates[0] = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};

    Graphics::Material material;
    material.textures.push_back({*iconTexture, Graphics::TextureMatrix()});
    display.Device().DrawQuad(quad, material);
}

void HomeButton::DrawBlack()
{
    Graphics::Canvas canvas = display.GetCanvas();
    float halfWidth = canvas.width / 2;
    float halfHeight = canvas.height / 2;

    Graphics::Quad quad = {};
    quad.positions = {{{-halfWidth, halfHeight}, {halfWidth, halfHeight}, {halfWidth, -halfHeight}, {-halfWidth, -halfHeight}}};
    quad.colors = {{Black, Black, Black, Black}};
    display.Device().DrawQuad(quad, Graphics::Material());
}

}
