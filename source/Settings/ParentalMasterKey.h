#pragma once

#include "Ios/Network/MacAddress.h"

#include <string>

namespace Settings
{

// When the user has forgotten both the PIN and the answer to the secret question, the page shows a confirmation number. Nintendo's
// support answered it with a master key that turns the parental controls off. The System Menu works both out itself (0x813FB774 and
// 0x813FC248, read to learn): the number comes from the address of the wireless card, the key from the number and the date of the day.

// The confirmation number: the address of the wireless card read as one number, its last eight decimal digits, as eight digits.
std::u16string ConfirmationNumber(const Ios::MacAddress& wirelessMacAddress);

// The master key that answers the confirmation number on that month (1 to 12) and day of the month, as five digits.
std::u16string MasterKey(const std::u16string& confirmationNumber, int month, int day);

}
