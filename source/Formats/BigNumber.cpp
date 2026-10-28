#include "Formats/BigNumber.h"

#include "Formats/FormatError.h"

#include <algorithm>
#include <format>

namespace Formats
{

namespace
{

constexpr int BitsPerWord = 32;
constexpr std::size_t BytesPerWord = 4;

// The number 2 less than the prime is the exponent that makes the inverse (Fermat's little theorem).
constexpr std::uint32_t Two = 2;

}

BigNumber::BigNumber(std::uint32_t value)
{
    if (value != 0)
        words.push_back(value);
}

BigNumber BigNumber::FromBigEndianBytes(ByteView bytes)
{
    BigNumber number;
    number.words.assign((bytes.size() + BytesPerWord - 1) / BytesPerWord, 0);

    for (std::size_t byteNumber = 0; byteNumber < bytes.size(); ++byteNumber)
    {
        std::size_t positionFromTheEnd = bytes.size() - 1 - byteNumber;
        std::size_t wordIndex = positionFromTheEnd / BytesPerWord;
        std::size_t shift = (positionFromTheEnd % BytesPerWord) * BitsPerByte;
        number.words[wordIndex] |= static_cast<std::uint32_t>(bytes[byteNumber]) << shift;
    }

    number.RemoveLeadingZeros();
    return number;
}

Bytes BigNumber::ToBigEndianBytes(std::size_t byteCount) const
{
    std::size_t neededByteCount = (BitCount() + BitsPerByte - 1) / BitsPerByte;
    if (neededByteCount > byteCount)
        throw FormatError(std::format("A number of {} bytes does not fit in {} bytes", neededByteCount, byteCount));

    Bytes bytes(byteCount, 0);
    for (std::size_t positionFromTheEnd = 0; positionFromTheEnd < neededByteCount; ++positionFromTheEnd)
    {
        std::size_t wordIndex = positionFromTheEnd / BytesPerWord;
        std::size_t shift = (positionFromTheEnd % BytesPerWord) * BitsPerByte;
        bytes[byteCount - 1 - positionFromTheEnd] = static_cast<Byte>((words[wordIndex] >> shift) & ByteMask);
    }

    return bytes;
}

bool BigNumber::IsZero() const
{
    return words.empty();
}

bool BigNumber::IsOdd() const
{
    return !words.empty() && (words.front() & 1) != 0;
}

std::size_t BigNumber::BitCount() const
{
    if (words.empty())
        return 0;

    std::size_t bitCount = (words.size() - 1) * BitsPerWord;
    std::uint32_t highestWord = words.back();
    while (highestWord != 0)
    {
        ++bitCount;
        highestWord >>= 1;
    }
    return bitCount;
}

bool BigNumber::GetBit(std::size_t bitIndex) const
{
    std::size_t wordIndex = bitIndex / BitsPerWord;
    if (wordIndex >= words.size())
        return false;

    return ((words[wordIndex] >> (bitIndex % BitsPerWord)) & 1) != 0;
}

std::strong_ordering BigNumber::operator<=>(const BigNumber& other) const
{
    if (words.size() != other.words.size())
        return words.size() <=> other.words.size();

    for (std::size_t index = words.size(); index > 0; --index)
    {
        if (words[index - 1] != other.words[index - 1])
            return words[index - 1] <=> other.words[index - 1];
    }

    return std::strong_ordering::equal;
}

bool BigNumber::operator==(const BigNumber& other) const
{
    return words == other.words;
}

BigNumber BigNumber::Add(const BigNumber& other) const
{
    BigNumber sum;
    std::size_t wordCount = std::max(words.size(), other.words.size());
    std::uint64_t carry = 0;

    for (std::size_t index = 0; index < wordCount; ++index)
    {
        std::uint64_t first = index < words.size() ? words[index] : 0;
        std::uint64_t second = index < other.words.size() ? other.words[index] : 0;
        std::uint64_t wordSum = first + second + carry;
        sum.words.push_back(static_cast<std::uint32_t>(wordSum));
        carry = wordSum >> BitsPerWord;
    }

    if (carry != 0)
        sum.words.push_back(static_cast<std::uint32_t>(carry));

    return sum;
}

BigNumber BigNumber::Subtract(const BigNumber& other) const
{
    if (*this < other)
        throw FormatError("A bigger number cannot be subtracted from a smaller one");

    BigNumber difference;
    std::int64_t borrow = 0;

    for (std::size_t index = 0; index < words.size(); ++index)
    {
        std::int64_t second = index < other.words.size() ? other.words[index] : 0;
        std::int64_t wordDifference = static_cast<std::int64_t>(words[index]) - second - borrow;
        borrow = wordDifference < 0 ? 1 : 0;
        if (wordDifference < 0)
            wordDifference += std::int64_t(1) << BitsPerWord;

        difference.words.push_back(static_cast<std::uint32_t>(wordDifference));
    }

    difference.RemoveLeadingZeros();
    return difference;
}

BigNumber BigNumber::Multiply(const BigNumber& other) const
{
    BigNumber product;
    product.words.assign(words.size() + other.words.size(), 0);

    for (std::size_t first = 0; first < words.size(); ++first)
    {
        std::uint64_t carry = 0;
        for (std::size_t second = 0; second < other.words.size(); ++second)
        {
            std::uint64_t partial = static_cast<std::uint64_t>(words[first]) * other.words[second] + product.words[first + second] + carry;
            product.words[first + second] = static_cast<std::uint32_t>(partial);
            carry = partial >> BitsPerWord;
        }
        product.words[first + other.words.size()] = static_cast<std::uint32_t>(carry);
    }

    product.RemoveLeadingZeros();
    return product;
}

BigNumber BigNumber::Remainder(const BigNumber& divisor) const
{
    if (divisor.IsZero())
        throw FormatError("A number cannot be divided by zero");

    // Long division in binary: take the bits of this number from the highest, and subtract the divisor whenever it fits.
    BigNumber remainder;
    remainder.words.reserve(divisor.words.size() + 1);

    for (std::size_t bitCount = BitCount(); bitCount > 0; --bitCount)
    {
        remainder.ShiftLeftByOneBit(GetBit(bitCount - 1));

        if (remainder >= divisor)
            remainder.SubtractInPlace(divisor);
    }

    return remainder;
}

BigNumber BigNumber::MultiplyModulo(const BigNumber& other, const BigNumber& modulus) const
{
    return Multiply(other).Remainder(modulus);
}

BigNumber BigNumber::PowerModulo(const BigNumber& exponent, const BigNumber& modulus) const
{
    BigNumber result = BigNumber(1).Remainder(modulus);
    BigNumber base = Remainder(modulus);

    // Square and multiply, from the highest bit of the exponent down.
    for (std::size_t bitCount = exponent.BitCount(); bitCount > 0; --bitCount)
    {
        result = result.MultiplyModulo(result, modulus);
        if (exponent.GetBit(bitCount - 1))
            result = result.MultiplyModulo(base, modulus);
    }

    return result;
}

BigNumber BigNumber::InverseModuloPrime(const BigNumber& prime) const
{
    if (Remainder(prime).IsZero())
        throw FormatError("Zero has no inverse");

    return PowerModulo(prime.Subtract(BigNumber(Two)), prime);
}

void BigNumber::ShiftLeftByOneBit(bool newLowestBit)
{
    std::uint32_t carry = newLowestBit ? 1 : 0;
    for (std::uint32_t& word : words)
    {
        std::uint32_t highestBit = word >> (BitsPerWord - 1);
        word = (word << 1) | carry;
        carry = highestBit;
    }

    if (carry != 0)
        words.push_back(carry);
}

void BigNumber::SubtractInPlace(const BigNumber& other)
{
    std::uint32_t borrow = 0;
    for (std::size_t index = 0; index < words.size(); ++index)
    {
        std::uint64_t second = static_cast<std::uint64_t>(index < other.words.size() ? other.words[index] : 0) + borrow;
        borrow = words[index] < second ? 1 : 0;
        words[index] = static_cast<std::uint32_t>(words[index] - second);
    }

    RemoveLeadingZeros();
}

void BigNumber::RemoveLeadingZeros()
{
    while (!words.empty() && words.back() == 0)
        words.pop_back();
}

}
