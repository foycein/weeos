#include "Formats/EllipticCurve.h"

#include "Formats/BigNumber.h"
#include "Formats/FormatError.h"
#include "Formats/HexText.h"

#include <array>
#include <bit>
#include <cstdint>
#include <utility>

namespace Formats
{

namespace
{

// The curve is y^2 + x*y = x^3 + a*x^2 + b over the field of 2^233 elements. A number of the field is a polynomial of degree 232 or less
// with coefficients 0 and 1, kept as 233 bits: bit i is the coefficient of x^i. The bits are in four words of 64 bits, the lowest word first.
// Adding two numbers is XOR. Multiplying them is done as polynomials, and then the result is reduced with the polynomial x^233 + x^74 + 1.

constexpr int WordCount = 4;
constexpr int BitsPerWord = 64;
constexpr int FieldBitCount = 233;
constexpr int ReductionTerm = 74;

struct Polynomial
{
    std::array<std::uint64_t, WordCount> words = {};

    bool operator==(const Polynomial&) const = default;
};

bool IsZero(const Polynomial& polynomial)
{
    return polynomial == Polynomial();
}

bool GetBit(const Polynomial& polynomial, int bitIndex)
{
    return ((polynomial.words[bitIndex / BitsPerWord] >> (bitIndex % BitsPerWord)) & 1) != 0;
}

void FlipBit(Polynomial& polynomial, int bitIndex)
{
    polynomial.words[bitIndex / BitsPerWord] ^= std::uint64_t(1) << (bitIndex % BitsPerWord);
}

// The highest exponent that has a coefficient of 1, or -1 for the polynomial zero.
int Degree(const Polynomial& polynomial)
{
    for (int wordIndex = WordCount - 1; wordIndex >= 0; --wordIndex)
    {
        std::uint64_t word = polynomial.words[wordIndex];
        if (word != 0)
            return wordIndex * BitsPerWord + (BitsPerWord - 1 - std::countl_zero(word));
    }
    return -1;
}

Polynomial Add(const Polynomial& first, const Polynomial& second)
{
    Polynomial sum;
    for (int index = 0; index < WordCount; ++index)
        sum.words[index] = first.words[index] ^ second.words[index];
    return sum;
}

// Multiplies by x to the power of count without reducing: every bit moves count places up, and bits beyond the four words are lost.
Polynomial ShiftLeft(const Polynomial& polynomial, int count)
{
    int wholeWords = count / BitsPerWord;
    int remainingBits = count % BitsPerWord;

    Polynomial shifted;
    for (int index = wholeWords; index < WordCount; ++index)
    {
        std::uint64_t word = polynomial.words[index - wholeWords];
        shifted.words[index] = word << remainingBits;

        bool hasLowerWord = index - wholeWords > 0;
        if (remainingBits != 0 && hasLowerWord)
            shifted.words[index] |= polynomial.words[index - wholeWords - 1] >> (BitsPerWord - remainingBits);
    }
    return shifted;
}

// Multiplies a number of the field by x. When the result has the term x^233, that term is replaced by x^74 + 1.
Polynomial MultiplyByX(const Polynomial& number)
{
    Polynomial result = ShiftLeft(number, 1);
    if (GetBit(result, FieldBitCount))
    {
        FlipBit(result, FieldBitCount);
        FlipBit(result, ReductionTerm);
        FlipBit(result, 0);
    }
    return result;
}

Polynomial Multiply(const Polynomial& first, const Polynomial& second)
{
    Polynomial product;
    Polynomial shiftedFirst = first;

    for (int bitIndex = 0; bitIndex < FieldBitCount; ++bitIndex)
    {
        if (GetBit(second, bitIndex))
            product = Add(product, shiftedFirst);

        shiftedFirst = MultiplyByX(shiftedFirst);
    }
    return product;
}

Polynomial MakeOne()
{
    Polynomial one;
    FlipBit(one, 0);
    return one;
}

// The number that gives 1 when multiplied by this one. The extended algorithm of Euclid: u and v are made smaller step by step,
// and what is done to them is also done to the numbers next to them, so that in the end the number next to u is the inverse.
Polynomial Invert(const Polynomial& number)
{
    Polynomial reduction;
    FlipBit(reduction, FieldBitCount);
    FlipBit(reduction, ReductionTerm);
    FlipBit(reduction, 0);

    const Polynomial one = MakeOne();
    Polynomial u = number;
    Polynomial v = reduction;
    Polynomial nextToU = one;
    Polynomial nextToV;

    while (u != one)
    {
        int shift = Degree(u) - Degree(v);
        if (shift < 0)
        {
            std::swap(u, v);
            std::swap(nextToU, nextToV);
            shift = -shift;
        }

        u = Add(u, ShiftLeft(v, shift));
        nextToU = Add(nextToU, ShiftLeft(nextToV, shift));
    }
    return nextToU;
}

Polynomial PolynomialFromBytes(ByteView bytes)
{
    Polynomial polynomial;
    for (std::size_t byteNumber = 0; byteNumber < bytes.size(); ++byteNumber)
    {
        std::size_t positionFromTheEnd = bytes.size() - 1 - byteNumber;
        polynomial.words[positionFromTheEnd / sizeof(std::uint64_t)] |=
            static_cast<std::uint64_t>(bytes[byteNumber]) << ((positionFromTheEnd % sizeof(std::uint64_t)) * BitsPerByte);
    }
    return polynomial;
}

Bytes PolynomialToBytes(const Polynomial& polynomial)
{
    Bytes bytes(EllipticCurveNumberSize);
    for (std::size_t positionFromTheEnd = 0; positionFromTheEnd < EllipticCurveNumberSize; ++positionFromTheEnd)
    {
        std::uint64_t word = polynomial.words[positionFromTheEnd / sizeof(std::uint64_t)];
        bytes[EllipticCurveNumberSize - 1 - positionFromTheEnd] =
            static_cast<Byte>((word >> ((positionFromTheEnd % sizeof(std::uint64_t)) * BitsPerByte)) & ByteMask);
    }
    return bytes;
}

// Reads a number of the curve. Fails for a number that has a coefficient above x^232.
bool TryReadFieldNumber(ByteView bytes, Polynomial& number)
{
    number = PolynomialFromBytes(bytes);
    return Degree(number) < FieldBitCount;
}

// The curve sect233r1, with the numbers of its standard (SEC 2, NIST B-233).
const Polynomial CurveA = MakeOne();
const Polynomial CurveB = PolynomialFromBytes(ParseHexText("0066647ede6c332c7f8c0923bb58213b333b20e9ce4281fe115f7d8f90ad"));
const Polynomial BaseX = PolynomialFromBytes(ParseHexText("00fac9dfcbac8313bb2139f1bb755fef65bc391f8b36f8f8eb7371fd558b"));
const Polynomial BaseY = PolynomialFromBytes(ParseHexText("01006a08a41903350678e58528bebf8a0beff867a7ca36716f7e01f81052"));

// The number of points that can be reached from the base point. It is a prime.
const BigNumber CurveOrder = BigNumber::FromBigEndianBytes(ParseHexText("01000000000000000000000000000013e974e72f8a6922031d2603cfe0d7"));

struct Point
{
    Polynomial x;
    Polynomial y;
    bool isInfinity = false;
};

Point MakeInfinity()
{
    Point point;
    point.isInfinity = true;
    return point;
}

Point MakeBasePoint()
{
    Point point;
    point.x = BaseX;
    point.y = BaseY;
    return point;
}

bool IsOnTheCurve(const Point& point)
{
    Polynomial xSquared = Multiply(point.x, point.x);
    Polynomial left = Add(Multiply(point.y, point.y), Multiply(point.x, point.y));
    Polynomial right = Add(Add(Multiply(xSquared, point.x), Multiply(CurveA, xSquared)), CurveB);
    return left == right;
}

Point Negate(const Point& point)
{
    if (point.isInfinity)
        return point;

    Point negated = point;
    negated.y = Add(point.x, point.y);
    return negated;
}

// The point that is the sum of a point with itself. A point with x = 0 is its own negative, so doubling it gives the point at infinity.
Point Double(const Point& point)
{
    if (point.isInfinity || IsZero(point.x))
        return MakeInfinity();

    Polynomial slope = Add(point.x, Multiply(point.y, Invert(point.x)));
    Polynomial newX = Add(Add(Multiply(slope, slope), slope), CurveA);
    Polynomial newY = Add(Multiply(point.x, point.x), Multiply(Add(slope, MakeOne()), newX));
    return Point{newX, newY, false};
}

Point AddPoints(const Point& first, const Point& second)
{
    if (first.isInfinity)
        return second;

    if (second.isInfinity)
        return first;

    if (first.x == second.x)
    {
        bool areEqual = first.y == second.y;
        return areEqual ? Double(first) : MakeInfinity();
    }

    Polynomial slope = Multiply(Add(first.y, second.y), Invert(Add(first.x, second.x)));
    Polynomial newX = Add(Add(Add(Multiply(slope, slope), slope), Add(first.x, second.x)), CurveA);
    Polynomial newY = Add(Add(Multiply(slope, Add(first.x, newX)), newX), first.y);
    return Point{newX, newY, false};
}

// The point added to itself as many times as the scalar says: double and add, from the highest bit of the scalar down.
Point MultiplyPoint(const Point& point, const BigNumber& scalar)
{
    Point result = MakeInfinity();
    for (std::size_t bitCount = scalar.BitCount(); bitCount > 0; --bitCount)
    {
        result = Double(result);
        if (scalar.GetBit(bitCount - 1))
            result = AddPoints(result, point);
    }
    return result;
}

Bytes PointToPublicKey(const Point& point)
{
    Bytes publicKey = PolynomialToBytes(point.x);
    Bytes y = PolynomialToBytes(point.y);
    publicKey.insert(publicKey.end(), y.begin(), y.end());
    return publicKey;
}

// Reads a public key. Gives nothing for bytes that are not a point of the curve.
std::optional<Point> TryReadPublicKey(ByteView publicKey)
{
    if (publicKey.size() != EllipticCurvePublicKeySize)
        return std::nullopt;

    Point point;
    bool xIsValid = TryReadFieldNumber(publicKey.first(EllipticCurveNumberSize), point.x);
    bool yIsValid = TryReadFieldNumber(publicKey.last(EllipticCurveNumberSize), point.y);
    if (!xIsValid || !yIsValid || !IsOnTheCurve(point))
        return std::nullopt;

    return point;
}

// An r or an s: a number from 1 to the order minus one.
std::optional<BigNumber> TryReadScalar(ByteView bytes)
{
    if (bytes.size() != EllipticCurveNumberSize)
        return std::nullopt;

    BigNumber scalar = BigNumber::FromBigEndianBytes(bytes);
    if (scalar.IsZero() || scalar >= CurveOrder)
        return std::nullopt;

    return scalar;
}

// A private key is 30 random bytes. Like the console, we take the remainder after division by the order, because the keys of
// consoles are not always smaller than the order. The key must not come out as zero.
std::optional<BigNumber> TryReadPrivateKey(ByteView bytes)
{
    if (bytes.size() != EllipticCurveNumberSize)
        return std::nullopt;

    BigNumber privateKey = BigNumber::FromBigEndianBytes(bytes).Remainder(CurveOrder);
    if (privateKey.IsZero())
        return std::nullopt;

    return privateKey;
}

BigNumber ReadHash(const Sha1Digest& hash)
{
    return BigNumber::FromBigEndianBytes(hash);
}

// The x coordinate of a point as a number modulo the order of the curve.
BigNumber XCoordinateModuloOrder(const Point& point)
{
    return BigNumber::FromBigEndianBytes(PolynomialToBytes(point.x)).Remainder(CurveOrder);
}

}

Bytes DerivePublicKey(ByteView privateKey)
{
    std::optional<BigNumber> scalar = TryReadPrivateKey(privateKey);
    if (!scalar)
        throw FormatError("An elliptic curve private key must be 30 bytes and not a multiple of the order of the curve");

    return PointToPublicKey(MultiplyPoint(MakeBasePoint(), *scalar));
}

std::optional<Bytes> SignHash(ByteView privateKey, const Sha1Digest& hash, ByteView nonce)
{
    std::optional<BigNumber> privateScalar = TryReadPrivateKey(privateKey);
    if (!privateScalar)
        throw FormatError("An elliptic curve private key must be 30 bytes and not a multiple of the order of the curve");

    BigNumber orderMinusOne = CurveOrder.Subtract(BigNumber(1));
    BigNumber nonceScalar = BigNumber::FromBigEndianBytes(nonce).Remainder(orderMinusOne).Add(BigNumber(1));

    BigNumber r = XCoordinateModuloOrder(MultiplyPoint(MakeBasePoint(), nonceScalar));
    if (r.IsZero())
        return std::nullopt;

    BigNumber hashPlusRTimesKey = ReadHash(hash).Add(r.MultiplyModulo(*privateScalar, CurveOrder));
    BigNumber s = nonceScalar.InverseModuloPrime(CurveOrder).MultiplyModulo(hashPlusRTimesKey, CurveOrder);
    if (s.IsZero())
        return std::nullopt;

    Bytes signature = r.ToBigEndianBytes(EllipticCurveNumberSize);
    Bytes sBytes = s.ToBigEndianBytes(EllipticCurveNumberSize);
    signature.insert(signature.end(), sBytes.begin(), sBytes.end());
    return signature;
}

bool VerifyHashSignature(ByteView publicKey, const Sha1Digest& hash, ByteView signature)
{
    if (signature.size() != EllipticCurveSignatureSize)
        return false;

    std::optional<Point> signer = TryReadPublicKey(publicKey);
    std::optional<BigNumber> r = TryReadScalar(signature.first(EllipticCurveNumberSize));
    std::optional<BigNumber> s = TryReadScalar(signature.last(EllipticCurveNumberSize));
    if (!signer || !r || !s)
        return false;

    BigNumber sInverse = s->InverseModuloPrime(CurveOrder);
    BigNumber hashFactor = ReadHash(hash).MultiplyModulo(sInverse, CurveOrder);
    BigNumber signerFactor = r->MultiplyModulo(sInverse, CurveOrder);

    Point sum = AddPoints(MultiplyPoint(MakeBasePoint(), hashFactor), MultiplyPoint(*signer, signerFactor));
    if (sum.isInfinity)
        return false;

    return XCoordinateModuloOrder(sum) == *r;
}

}
