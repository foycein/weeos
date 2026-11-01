#include "SystemMenu/TurnOffQuestion.h"

namespace SystemMenu
{

namespace
{

constexpr std::u16string_view QuestionText = u"Turn off WeeOS?";
constexpr std::u16string_view YesText = u"Yes";
constexpr std::u16string_view NoText = u"No";

constexpr DialogAnswer YesButton = DialogAnswer::ButtonA;
constexpr DialogAnswer NoButton = DialogAnswer::ButtonB;

}

TurnOffQuestion::TurnOffQuestion(Display& display, const MenuFiles& files, SoundRequests& sounds)
    : dialog(display, files, sounds)
{
}

void TurnOffQuestion::Ask()
{
    if (!dialog.IsOpen())
        isAsked = true;
}

bool TurnOffQuestion::IsOpen() const
{
    return isAsked || dialog.IsOpen();
}

bool TurnOffQuestion::Update(const InputManager& input)
{
    if (isAsked)
        OpenTheDialog();

    constexpr bool isActive = true;
    dialog.Update(input, isActive);

    std::optional<DialogAnswer> answer = dialog.TakeAnswer();
    return answer == YesButton;
}

void TurnOffQuestion::OpenTheDialog()
{
    isAsked = false;

    DialogRequest request;
    request.layout = DialogLayout::TwoButtons;
    request.message = QuestionText;
    request.buttonAText = YesText;
    request.buttonBText = NoText;
    request.cancelButton = NoButton;
    dialog.Open(request);
}

void TurnOffQuestion::Draw()
{
    dialog.Draw();
}

}
