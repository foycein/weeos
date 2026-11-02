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
//   IPL.INC    the number of tiles in the channel grid, IPL.FRC the number of free slots: the Disc Channel and the six channels the
//              import keeps (7 and 41 of the 48 slots), because the console's downloaded channels are left out
//   NET.PROF   the network profile (addresses, proxy, PPPoE user and password) and NET.CNF: removed
//
// A setting that is not in the configuration is not added. Throws a FormatError when IPL.NIK or IPL.PC has another size
// than the console makes it, because then the places of the personal values are not known.
void ResetPersonalSettings(Formats::SystemConfiguration& configuration);

// The network configuration that WeeOS's NAND holds instead of the console's own (/shared2/sys/net/02/config.dat): Connection 1
// is a wired connection with automatic settings, switched on, chosen, and with its connection test passed: the PC's connection, through
// which WeeOS is online. The file decides nothing; it is there for the Internet pages. Connections 2 and 3 are empty.
Formats::Bytes MakeWeeOSNetworkConfigurationFile();

// Reads the bytes of a SYSCONF file, resets the personal settings and writes the file again.
Formats::Bytes ResetPersonalSettingsOfFile(Formats::ByteView systemConfigurationBytes);
