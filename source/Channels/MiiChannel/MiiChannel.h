#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/MiiChannel/EditorScreen.h"
#include "Channels/MiiChannel/LookAlikeScreen.h"
#include "Channels/MiiChannel/MessageWindow.h"
#include "Channels/MiiChannel/MiiChannelData.h"
#include "Channels/MiiChannel/MiiChannelFiles.h"
#include "Channels/MiiChannel/MiiFigures.h"
#include "Channels/MiiChannel/ParadeScreen.h"
#include "Channels/MiiChannel/PlazaGround.h"
#include "Channels/MiiChannel/PlazaScreen.h"
#include "Channels/MiiChannel/ProfileDialog.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/SoftKeyboard.h"

#include <memory>
#include <optional>

namespace Channels
{

// The Mii Channel (HACA v6) without a network (docs/audit/MiiChannel.md): the plaza with the Miis of RFL_DB.dat, making a Mii from scratch
// or from a look-alike, the editor with every page, saving and erasing, and the offline answers of the transfers and of the Mii Parade.
class MiiChannel : public Channel
{
public:
    enum class Scene
    {
        // The broken database's question, or the welcome of the first start.
        Starting,
        Plaza,

        // "Select a gender." and "How do you want to make a Mii?"
        NewMiiQuestions,
        LookAlike,
        Editor,
        Parade,
    };

    // The services must outlive the channel. Throws when a file of the title is missing or the NAND cannot be read.
    explicit MiiChannel(ChannelServices& channelServices);

    void Update() override;
    void Draw() override;
    SystemMenu::CursorManager& Hands() override;
    EffectHands* OwnHands() override;

    Scene CurrentScene() const;

    // The parts, for the tests.
    PlazaScreen& Plaza();
    ParadeScreen& Parade();
    EditorScreen& Editor();
    LookAlikeScreen& LookAlike();
    MessageWindow& Message();
    ProfileDialog& Profile();
    MiiChannelData& Data();
    MiiEffects& Effects();

private:
    enum class Welcome
    {
        None,
        BrokenDatabase,
        Greeting,
        Explaining,
        ThereIsYourMii,
        HelpAndHomeButton,
    };

    void StartFirstScene();
    void UpdateScene(bool acceptsInput);
    void FollowWelcome();
    void FollowPlaza();
    void FollowNewMiiQuestions();
    void FollowLookAlike();
    void FollowEditor();
    void FollowParade();
    void AskNewMiiQuestion(ProfileQuestion question);
    Mii::MiiRecord AsNewMiiOfThisConsole(Mii::MiiRecord record) const;
    void ShowPlaza();
    void PlayKeyboardSounds();

    ChannelServices& services;
    MiiChannelFiles files;
    SystemMenu::MenuFonts fonts;
    MiiChannelData data;
    MiiFigures figures;
    MiiEffects effects;
    PlazaGround ground;
    PlazaGround road;
    MessageWindow message;
    ProfileDialog profile;
    SystemMenu::SoundRequests keyboardSounds;
    SystemMenu::SoftKeyboard keyboard;
    PlazaScreen plaza;
    ParadeScreen parade;
    LookAlikeScreen lookAlike;
    EditorScreen editor;
    std::unique_ptr<SystemMenu::CursorManager> hands;
    EffectHands ownHands;

    Scene scene = Scene::Starting;
    Welcome welcome = Welcome::None;
    std::optional<ProfileQuestion> askedQuestion;
    Mii::Gender newMiiGender = Mii::Gender::Male;
    bool isMakingFirstMii = false;
};

}
