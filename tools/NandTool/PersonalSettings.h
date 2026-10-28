#pragma once

#include "Formats/Bytes.h"
#include "Formats/SystemConfiguration.h"

// Sets the settings of a system configuration that belong to the person who used the console back to the values of a console
// that just left the factory. The other settings, like the language, the aspect ratio, the sensor bar position and the sound
// volume of the Wii Remote, are the system's and stay as they are.
//
//   IPL.NIK    the nickname of the console: empty
//   IPL.PC     the parental controls: off, no PIN, no secret question and answer
//   BT.DINF    the Wii Remotes synced to the console, with their addresses and link keys: none
//   BT.CDIF    the guest Wii Remotes: none
//   IPL.SADR   the address: only the country stays, the region, the city and the coordinates are cleared
//   IPL.CB     the difference between the clock chip and the local time: none, the clock of the product is the PC's
//   IPL.TID    the title of a channel installed from the SD card for a moment: none
//   IPL.INC    the number of channels in the channel grid, IPL.FRC the number of free slots: the five channels the import keeps
//              (5 and 43 of the 48 slots), because the downloaded channels of the owner are left out
//   NET.PROF   the network profile (addresses, proxy, PPPoE user and password) and NET.CNF: removed
//
// A setting that is not in the configuration is not added. Throws a FormatError when IPL.NIK or IPL.PC has another size
// than the console makes it, because then the places of the personal values are not known.
void ResetPersonalSettings(Formats::SystemConfiguration& configuration);

// Reads the bytes of a SYSCONF file, resets the personal settings and writes the file again.
Formats::Bytes ResetPersonalSettingsOfFile(Formats::ByteView systemConfigurationBytes);
