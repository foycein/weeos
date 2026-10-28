#pragma once

#include "Formats/TitleMetadata.h"
#include "Ios/Profile/IosProfile.h"

#include <stdexcept>
#include <string>

namespace Ios
{

// Why no profile can be chosen for a title.
enum class ProfileProblem
{
    // The title names no IOS: its metadata has no required IOS and the title is not an IOS itself.
    NamesNoIos,

    // The IOS is not one of the IOS titles of the console.
    NotInstalled,

    // The IOS is an empty stub. It has no devices, so no title can run on it.
    EmptyStub,
};

// A profile cannot be chosen. The message says in plain words which title or IOS and why.
class ProfileError : public std::runtime_error
{
public:
    ProfileError(ProfileProblem problem, const std::string& message);

    ProfileProblem Problem() const;

private:
    ProfileProblem problem;
};

// The profile of an IOS. Throws a ProfileError when the console has no such IOS or when the IOS is a stub.
IosProfile ChooseProfile(IosNumber number);

// The profile of the IOS that a title runs on. The title metadata names the IOS in its required system version,
// for example 00000001-00000050 for IOS80. An IOS title names no other IOS: it runs on itself.
// Throws a ProfileError when the title names no IOS, when the IOS is not installed or when it is a stub.
IosProfile ChooseProfileForTitle(const Formats::TitleMetadata& metadata);

// The profile of the System Menu: IOS80 version 7200. The metadata of the System Menu names IOS80.
IosProfile SystemMenuProfile();

}
