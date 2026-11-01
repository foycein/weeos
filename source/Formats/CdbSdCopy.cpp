#include "Formats/CdbSdCopy.h"

#include "Formats/CdbRecordFile.h"
#include "Formats/FormatError.h"
#include "Formats/HmacSha1.h"

#include <algorithm>
#include <format>

namespace Formats
{

namespace
{

constexpr AesKey CopyKey{};

void PutBigEndianWord(Bytes& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset] = static_cast<Byte>(value >> 24);
    bytes[offset + 1] = static_cast<Byte>(value >> 16);
    bytes[offset + 2] = static_cast<Byte>(value >> 8);
    bytes[offset + 3] = static_cast<Byte>(value);
}

std::size_t RoundUpToWholePieces(std::size_t size)
{
    std::size_t pieceCount = (size + CdbSdCopyPieceSize - 1) / CdbSdCopyPieceSize;
    return pieceCount * CdbSdCopyPieceSize;
}

}

Bytes MakeCdbSdCopy(ByteView recordFile, std::string_view recordKey, const AesBlock& initializationVector, ByteView hmacKey)
{
    if (recordFile.size() < CdbRecordHeaderSize)
        throw FormatError(std::format("CDB copy: a record of {} bytes has no whole header", recordFile.size()));
    if (recordKey.size() >= CdbSdCopyRecordKeySize)
        throw FormatError(std::format("CDB copy: the key {} is too long", recordKey));

    Bytes copy(recordFile.begin(), recordFile.begin() + CdbRecordHeaderSize);
    PutBigEndianWord(copy, CdbSdCopyFileSizeOffset, static_cast<std::uint32_t>(recordFile.size()));
    std::fill_n(copy.begin() + CdbSdCopyRecordKeyOffset, CdbSdCopyRecordKeySize, Byte{0});
    std::copy(recordKey.begin(), recordKey.end(), copy.begin() + CdbSdCopyRecordKeyOffset);
    std::copy(initializationVector.begin(), initializationVector.end(), copy.begin() + CdbSdCopyInitializationVectorOffset);
    std::fill_n(copy.begin() + CdbSdCopyHmacOffset, CdbSdCopyHmacAreaSize, Byte{0});

    Bytes data(recordFile.begin() + CdbRecordHeaderSize, recordFile.end());
    data.resize(RoundUpToWholePieces(data.size()), 0);
    Bytes encryptedData = EncryptAes128Cbc(data, CopyKey, initializationVector);
    copy.insert(copy.end(), encryptedData.begin(), encryptedData.end());

    Sha1Digest hmac = ComputeHmacSha1(hmacKey, copy);
    std::copy(hmac.begin(), hmac.end(), copy.begin() + CdbSdCopyHmacOffset);
    return copy;
}

}
