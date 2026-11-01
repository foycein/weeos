#pragma once

#include "Mii/MiiRecord.h"

#include <random>

namespace Mii
{

enum class MiiAge
{
    Child,
    Adult,
    Elder,
};

enum class MiiLook
{
    DarkSkin,
    LightSkin,
    EastAsian,
};

// Makes a random Mii the way the face library does (docs/research/Mii.md section 4), for example for the "Choose a look-alike"
// screen of the Mii Channel. Every part is drawn from the tables of its kind of Mii. The Mii is named "no name" and has a
// temporary create id, so it cannot be stored as it is.
MiiRecord MakeRandomMii(Gender gender, MiiAge age, MiiLook look, std::mt19937& random);

// What the face library chooses when a program asks for any gender, age or look.
Gender ChooseRandomGender(std::mt19937& random);
MiiAge ChooseRandomAge(std::mt19937& random);
MiiLook ChooseRandomLook(std::mt19937& random);

}
