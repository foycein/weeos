#include "SystemMenu/MenuSystem.h"

#include "SystemMenu/FirstScreen.h"
#include "SystemMenu/ScreenFactory.h"

namespace SystemMenu
{

MenuSystem::MenuSystem(Graphics::GraphicsDevice& device, TitleContents& titleContents, Ios::RemoteService& remoteService,
    Platform::ScreenShape screenShape, SettingsFlowServices* wiiSettingsServices)
    : display(device, screenShape)
    , files(titleContents)
    , fonts(device, titleContents, files)
    , messages(files.OpenEnglishMessages())
    , fader(display)
    , input(remoteService)
    , cursors(display, files)
    , screens([this](ScreenId screen, int argument) { return MakeScreen(screen, argument, services); })
    , keyboard(display, files, messages, clock, input, sounds)
    , services{display, files, messages, clock, fader, input, cursors, screens, sounds, keyboard, wiiSettingsServices}
{
    display.SetTextFonts(fonts.Fonts());

    FirstScreen firstScreen = ChooseFirstScreen();
    screens.Start(firstScreen.screen, firstScreen.argument);
}

void MenuSystem::Update()
{
    sounds.StartNextStep();
    clock.Advance();
    input.Poll();
    screens.Update();
    keyboard.Update();
    cursors.Update(input);
    fader.Calculate();
}

void MenuSystem::Draw()
{
    display.BeginPicture();
    DrawScene();
}

void MenuSystem::DrawScene()
{
    screens.Draw();
    keyboard.Draw();
    cursors.Draw();
    fader.Draw();
}

void MenuSystem::UsePhysicalKeyboard(Ios::KeyboardService& keyboardService)
{
    keyboard.UsePhysicalKeyboard(keyboardService);
}

MenuServices& MenuSystem::Services()
{
    return services;
}

}
