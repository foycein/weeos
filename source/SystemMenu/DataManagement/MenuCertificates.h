#pragma once

#include "SystemMenu/ProgramImage.h"

#include "Formats/Bytes.h"

namespace SystemMenu
{

// The four certificates that the System Menu keeps in its program to check the signature of save data from an SD card:
// CA00000001 and MS00000002, which vouch for the consoles, and CA00000002 and MS00000003, one after the other.
Formats::Bytes ReadMenuCertificates(const ProgramImage& program);

}
