#pragma once

#include "Formats/Bytes.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Formats
{

// A whole number that is not negative and can be as big as the keys and signatures of the Wii need: 4096 bits and more.
// It is made to be read, not to be fast. Every operation is a few plain lines.
class BigNumber
{
public:
    // Zero.
    BigNumber() = default;

    explicit BigNumber(std::uint32_t value);

    // The number whose digits are the bytes, the most significant byte first.
    static BigNumber FromBigEndianBytes(ByteView bytes);

    // The bytes of the number, the most significant first, with zeros in front to make byteCount bytes.
    // Throws a FormatError when the number needs more bytes.
    Bytes ToBigEndianBytes(std::size_t byteCount) const;

    bool IsZero() const;
    bool IsOdd() const;

    // The number of bits up to the highest bit that is set. Zero for the number zero.
    std::size_t BitCount() const;

    bool GetBit(std::size_t bitIndex) const;

    std::strong_ordering operator<=>(const BigNumber& other) const;
    bool operator==(const BigNumber& other) const;

    BigNumber Add(const BigNumber& other) const;

    // Throws a FormatError when the other number is bigger.
    BigNumber Subtract(const BigNumber& other) const;

    BigNumber Multiply(const BigNumber& other) const;

    // What is left when this number is divided by the divisor as often as possible. The divisor must not be zero.
    BigNumber Remainder(const BigNumber& divisor) const;

    BigNumber MultiplyModulo(const BigNumber& other, const BigNumber& modulus) const;

    // This number to the power of the exponent, with the remainder after division by the modulus.
    BigNumber PowerModulo(const BigNumber& exponent, const BigNumber& modulus) const;

    // The number that gives 1 when multiplied by this one, with the remainder after division by the prime. This number must not be zero.
    BigNumber InverseModuloPrime(const BigNumber& prime) const;

private:
    // Moves every bit one place up. The bit that comes in at the bottom is newLowestBit.
    void ShiftLeftByOneBit(bool newLowestBit);

    // Takes the other number away. This number must not be smaller.
    void SubtractInPlace(const BigNumber& other);

    // Each element holds 32 bits of the number, the least significant first. The last element is never zero.
    void RemoveLeadingZeros();

    std::vector<std::uint32_t> words;
};

}
