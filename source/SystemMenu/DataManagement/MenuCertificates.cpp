#include "SystemMenu/DataManagement/MenuCertificates.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace SystemMenu
{

namespace
{

struct CertificatePlace
{
    std::uint32_t address;
    std::size_t size;
};

// Where the program keeps them (the menu passes them to ES_VerifySign at 0x815C47E0).
constexpr std::array<CertificatePlace, 4> CertificatePlaces = {{
    {0x8162DBB8, 0x400},
    {0x8162E1F8, 0x240},
    {0x8162D7B8, 0x400},
    {0x8162DFB8, 0x240},
}};

}

Formats::Bytes ReadMenuCertificates(const ProgramImage& program)
{
    Formats::Bytes certificates;
    for (const CertificatePlace& place : CertificatePlaces)
    {
        Formats::ByteView bytes = program.BytesAt(place.address).first(place.size);
        certificates.insert(certificates.end(), bytes.begin(), bytes.end());
    }
    return certificates;
}

}
