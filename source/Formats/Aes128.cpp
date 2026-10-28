#include "Formats/Aes128.h"

#include "Formats/FormatError.h"

#include <format>

namespace Formats
{

namespace
{

constexpr int RowCount = 4;
constexpr int ColumnCount = 4;
constexpr int ByteValueCount = 256;
constexpr Byte HighestBit = 0x80;
constexpr Byte FieldReductionBits = 0x1B;

using ByteTable = std::array<Byte, ByteValueCount>;

const ByteTable SubstitutionBox = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16,
};

constexpr std::size_t LastWordOffset = 12;

// The round constants are used while the key is expanded. Each one is the previous one doubled in the field.
constexpr std::array<Byte, Aes128RoundCount> RoundConstants = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36};

Byte DoubleInField(Byte value)
{
    Byte doubled = static_cast<Byte>(value << 1);
    if (value & HighestBit)
        doubled ^= FieldReductionBits;
    return doubled;
}

Byte MultiplyInField(Byte value, Byte factor)
{
    Byte product = 0;
    while (factor != 0)
    {
        if (factor & 1)
            product ^= value;

        value = DoubleInField(value);
        factor >>= 1;
    }
    return product;
}

ByteTable BuildMultiplicationTable(Byte factor)
{
    ByteTable table;
    for (int value = 0; value < ByteValueCount; ++value)
        table[value] = MultiplyInField(static_cast<Byte>(value), factor);
    return table;
}

ByteTable BuildInverseSubstitutionBox()
{
    ByteTable inverse;
    for (int value = 0; value < ByteValueCount; ++value)
        inverse[SubstitutionBox[value]] = static_cast<Byte>(value);
    return inverse;
}

const ByteTable InverseSubstitutionBox = BuildInverseSubstitutionBox();
const ByteTable TimesNine = BuildMultiplicationTable(9);
const ByteTable TimesEleven = BuildMultiplicationTable(11);
const ByteTable TimesThirteen = BuildMultiplicationTable(13);
const ByteTable TimesFourteen = BuildMultiplicationTable(14);

// The state holds the 16 bytes of a block column by column: the byte of a row and column is at row + 4 * column.
int StateIndex(int row, int column)
{
    return row + RowCount * column;
}

void AddRoundKey(AesBlock& state, const AesBlock& roundKey)
{
    for (std::size_t index = 0; index < AesBlockSize; ++index)
        state[index] ^= roundKey[index];
}

void InverseShiftRows(AesBlock& state)
{
    AesBlock shifted;
    for (int row = 0; row < RowCount; ++row)
    {
        for (int column = 0; column < ColumnCount; ++column)
        {
            int shiftedColumn = (column + row) % ColumnCount;
            shifted[StateIndex(row, shiftedColumn)] = state[StateIndex(row, column)];
        }
    }
    state = shifted;
}

void InverseSubstituteBytes(AesBlock& state)
{
    for (Byte& value : state)
        value = InverseSubstitutionBox[value];
}

void InverseMixColumns(AesBlock& state)
{
    for (int column = 0; column < ColumnCount; ++column)
    {
        Byte first = state[StateIndex(0, column)];
        Byte second = state[StateIndex(1, column)];
        Byte third = state[StateIndex(2, column)];
        Byte fourth = state[StateIndex(3, column)];

        state[StateIndex(0, column)] = TimesFourteen[first] ^ TimesEleven[second] ^ TimesThirteen[third] ^ TimesNine[fourth];
        state[StateIndex(1, column)] = TimesNine[first] ^ TimesFourteen[second] ^ TimesEleven[third] ^ TimesThirteen[fourth];
        state[StateIndex(2, column)] = TimesThirteen[first] ^ TimesNine[second] ^ TimesFourteen[third] ^ TimesEleven[fourth];
        state[StateIndex(3, column)] = TimesEleven[first] ^ TimesThirteen[second] ^ TimesNine[third] ^ TimesFourteen[fourth];
    }
}

AesBlock CopyBlock(ByteView bytes, std::size_t offset)
{
    AesBlock block;
    for (std::size_t index = 0; index < AesBlockSize; ++index)
        block[index] = bytes[offset + index];
    return block;
}

}

Aes128Decryptor::Aes128Decryptor(const AesKey& key)
{
    roundKeys[0] = key;

    for (int round = 1; round <= Aes128RoundCount; ++round)
    {
        const AesBlock& previous = roundKeys[round - 1];
        AesBlock& current = roundKeys[round];

        // The first word combines the previous first word with the last word of the previous key,
        // rotated by one byte and substituted.
        current[0] = previous[0] ^ SubstitutionBox[previous[LastWordOffset + 1]] ^ RoundConstants[round - 1];
        current[1] = previous[1] ^ SubstitutionBox[previous[LastWordOffset + 2]];
        current[2] = previous[2] ^ SubstitutionBox[previous[LastWordOffset + 3]];
        current[3] = previous[3] ^ SubstitutionBox[previous[LastWordOffset]];

        for (std::size_t index = 4; index < AesBlockSize; ++index)
            current[index] = previous[index] ^ current[index - 4];
    }
}

AesBlock Aes128Decryptor::DecryptBlock(const AesBlock& encryptedBlock) const
{
    AesBlock state = encryptedBlock;

    AddRoundKey(state, roundKeys[Aes128RoundCount]);

    for (int round = Aes128RoundCount - 1; round >= 1; --round)
    {
        InverseShiftRows(state);
        InverseSubstituteBytes(state);
        AddRoundKey(state, roundKeys[round]);
        InverseMixColumns(state);
    }

    InverseShiftRows(state);
    InverseSubstituteBytes(state);
    AddRoundKey(state, roundKeys[0]);

    return state;
}

Bytes DecryptAes128Cbc(ByteView encryptedBytes, const AesKey& key, const AesBlock& initializationVector)
{
    if (encryptedBytes.size() % AesBlockSize != 0)
    {
        throw FormatError(std::format("AES data must be a multiple of {} bytes, but it has {} bytes",
            AesBlockSize, encryptedBytes.size()));
    }

    Aes128Decryptor decryptor(key);
    Bytes decryptedBytes(encryptedBytes.size());
    AesBlock previousEncryptedBlock = initializationVector;

    for (std::size_t offset = 0; offset < encryptedBytes.size(); offset += AesBlockSize)
    {
        AesBlock encryptedBlock = CopyBlock(encryptedBytes, offset);
        AesBlock decryptedBlock = decryptor.DecryptBlock(encryptedBlock);

        for (std::size_t index = 0; index < AesBlockSize; ++index)
            decryptedBytes[offset + index] = decryptedBlock[index] ^ previousEncryptedBlock[index];

        previousEncryptedBlock = encryptedBlock;
    }

    return decryptedBytes;
}

}
