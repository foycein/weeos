#pragma once

#include "Ios/IosServices.h"

namespace WeeOS
{

// The console after its start-up: boot2 has picked the System Menu, IOS runs on the IOS that the System Menu names,
// and the System Menu is the title that runs. This is the one place of the program that knows the order of the start-up.
class Console
{
public:
    // Throws a Boot::BootError when something is missing on the NAND or IOS cannot start, and a ProfileError when the IOS is not usable.
    // The messages say in plain words what is wrong.
    explicit Console(const Ios::IosFolders& folders);
    ~Console();

    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

    Ios::IosServices& Services();

private:
    Ios::IosServices services;
};

}
