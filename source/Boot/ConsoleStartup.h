#pragma once

#include "Boot/BootInformation.h"
#include "Ios/IosFolders.h"
#include "Ios/IosServices.h"
#include "Ios/Title/TicketView.h"
#include "Ios/Title/TitleId.h"

#include <optional>

namespace Boot
{

// The console from the power button on. A program makes one of these, powers it on and runs the System Menu on the services it gives.
//
//     Boot::ConsoleStartup console(folders);
//     const Boot::BootInformation& information = console.PowerOn();
//     Ios::IosServices& services = console.GetServices();
//     ... run the System Menu; when it starts a channel:
//     console.LaunchTitle(channelId, ticketView);
//
// The sequence is the one of the console (docs/research/Boot2.md, docs/research/BootChain.md):
//  - boot2 starts. It has a file system and the title service. It prepares the NAND folders, decides that the System Menu starts
//    (BootSelection) and asks for it like any title is asked for: the request goes to /sys/launch.sys and IOS restarts.
//  - The new IOS, the one the System Menu needs, reads the request, checks it and enters the title.
//  - The program gets the services and the facts that IOS leaves for the PowerPC.
// Every start of a title later is the second half of this.
//
// Not thread safe: one thread powers the console on, launches titles and powers it off.
class ConsoleStartup
{
public:
    // The NAND folder and the folder with the identity of the console (keys.txt, device.cert and seeprom.bin of a console dump).
    // The check tells the network services of IOS whether the PC is connected.
    explicit ConsoleStartup(Ios::IosFolders iosFolders, Ios::PcConnectionCheck pcConnectionCheck = Ios::IsWindowsConnected);

    // Powers the console off.
    ~ConsoleStartup();

    ConsoleStartup(const ConsoleStartup&) = delete;
    ConsoleStartup& operator=(const ConsoleStartup&) = delete;

    // Runs boot2, brings up IOS and enters the System Menu. Throws a BootError that says in plain words what is wrong
    // (no System Menu or no IOS on the NAND, a ticket that does not fit, an identity that cannot be read). The console stays off then.
    // Throws std::logic_error when the console is on already.
    const BootInformation& PowerOn();

    // True while IOS runs: from a successful PowerOn until PowerOff, also while IOS runs with no title because a launch failed
    // after the restart. False after a launch that failed to bring IOS up again.
    bool IsOn() const;

    // The services of the IOS that runs. Everything a program holds of them is invalid after LaunchTitle and PowerOff.
    // Throws std::logic_error when the console is off.
    Ios::IosServices& GetServices();

    // The facts about the title that runs. Throws std::logic_error when no title runs: the console is off, or a launch ended
    // the IOS that ran and then failed.
    const BootInformation& GetBootInformation() const;

    // The running program asks for another title, for example a channel, or the System Menu again.
    // IOS restarts with the IOS that the title needs. The program has ended everything that uses the services before it calls.
    // Throws a BootError when the title cannot start. See TitleLauncher for what that leaves.
    const BootInformation& LaunchTitle(Ios::TitleId titleId, const Ios::TicketView& ticketView);

    // The power button of the console. IOS ends, so whoever waits for an event stops waiting. The NAND stays as it is.
    void PowerOff();

private:
    void RequireOn() const;

    Ios::IosFolders folders;
    Ios::IosServices services;
    std::optional<BootInformation> bootInformation;
};

}
