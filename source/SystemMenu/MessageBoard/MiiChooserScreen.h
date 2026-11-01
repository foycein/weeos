#pragma once

#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/MessageBoard/BoardPosting.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/ScreenId.h"

#include "Layout/LayoutArchive.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace SystemMenu
{

struct MessageBoardData;

constexpr std::size_t MiisPerPage = 10;

// The Mii chooser (screen 0x1C, the class 0x813BB0A4 to 0x813BD5D0, faceSel.ash): the Miis of the console's database, ten to a page,
// with arrows for the pages. The Mii that is pressed goes to the memo writer (MessageBoardData::chosenMii); B or Back leaves without one.
class MiiChooserScreen : public Screen
{
public:
    explicit MiiChooserScreen(MenuServices& menuServices);

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

    int Page() const;
    int PageCount() const;
    std::optional<Formats::Vector2> PlaceOfMii(std::size_t miiPlace);

protected:
    bool RunMain() override;

private:
    void ShowThePage();
    void RunChoosing();
    std::optional<std::size_t> MiiPlaceAt(Formats::Vector2 place);
    bool HandleArrowPress(Formats::Vector2 place);
    void TurnPage(int direction);
    void Leave(std::optional<MiiData> chosenMii);

    MenuServices& services;
    MessageBoardData& data;
    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<MenuLayout> layout;
    std::vector<MiiData> miis;
    int page = 0;
    std::size_t appearAnimation = 0;
    std::size_t leaveAnimation = 0;
    std::size_t arrowLoopAnimation = 0;
    std::array<bool, PlayerCount> wasOnAMii{};
    bool isLeaving = false;
};

}
