#include "SystemMenu/MenuSystem.h"

#include "SystemMenu/FirstScreen.h"
#include "SystemMenu/ScreenFactory.h"

namespace SystemMenu
{

MenuSystem::MenuSystem(Graphics::GraphicsDevice& device, TitleContents& titleContents, Ios::RemoteService& remoteService,
    Platform::ScreenShape screenShape, SettingsFlowServices* wiiSettingsServices)
    : display(device, screenShape)
    , files(titleContents)
    , messages(files.OpenEnglishMessages())
    , fader(display)
    , input(remoteService)
    , cursors(display, files)
    , screens([this](ScreenId screen, int argument) { return MakeScreen(screen, argument, services); })
    , services{display, files, messages, clock, fader, input, cursors, screens, wiiSettingsServices}
{
    FirstScreen firstScreen = ChooseFirstScreen();
    screens.Start(firstScreen.screen, firstScreen.argument);
}

void MenuSystem::Update()
{
    clock.Advance();
    input.Poll();
    screens.Update();
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
    cursors.Draw();
    fader.Draw();
}

MenuServices& MenuSystem::Services()
{
    return services;
}

}
