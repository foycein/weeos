#pragma once

#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/FatalError.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/ProgramImage.h"

#include "Formats/MessageFile.h"
#include "Layout/LayoutArchive.h"

#include <memory>

namespace SystemMenu
{

// The screen that tells the user that the console cannot go on (the ErrorScreen of the original).
// Its layout, its font and its texts are not in files of the title but inside the main program, so it works when the files fail.
// It is not a screen of the tree: it stops the whole menu. The picture fades to black, the text fades in, and the user switches the
// console off or resets it.
class ErrorScreen
{
public:
    // Reads the layout, the font and the texts from the main program. Everything given must stay alive as long as this object.
    ErrorScreen(Display& menuDisplay, const ProgramImage& program, MenuFonts& menuFonts, Fader& screenFader);

    // Shows the text of the reason. Does nothing when an error is shown already.
    void Show(FatalError reason);

    // True from Show on: the menu stops.
    bool IsRequested() const;

    // True when the text has begun to fade in: the menu is not drawn any more.
    bool IsShown() const;

    // Once in every step while the error is requested.
    void Update();

    void Draw();

private:
    enum class State
    {
        Off,
        Requested,
        Showing,
    };

    Display& display;
    Fader& fader;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    Formats::MessageFile texts;
    State state = State::Off;
};

}
