#pragma once

#include "Formats/Bytes.h"

#include <array>
#include <cstddef>

namespace Formats
{

constexpr std::size_t AesBlockSize = 16;
constexpr int Aes128RoundCount = 10;

using AesBlock = std::array<Byte, AesBlockSize>;
using AesKey = AesBlock;

class Aes128Decryptor
{
public:
    explicit Aes128Decryptor(const AesKey& key);

    AesBlock DecryptBlock(const AesBlock& encryptedBlock) const;

private:
    std::array<AesBlock, Aes128RoundCount + 1> roundKeys;
};

// Cipher block chaining: each block is decrypted and then combined with the block of encrypted data before it.
// The size of the encrypted data must be a multiple of the block size.
Bytes DecryptAes128Cbc(ByteView encryptedBytes, const AesKey& key, const AesBlock& initializationVector);

}
