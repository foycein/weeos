#include "Formats/CdbSdCopy.h"

#include "Formats/Alignment.h"
#include "Formats/ByteWriter.h"
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
    ByteWriter word;
    word.WriteUint32(value);
    std::copy(word.GetBytes().begin(), word.GetBytes().end(), bytes.begin() + offset);
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
    data.resize(RoundUpToMultiple(data.size(), CdbSdCopyPieceSize), 0);
    Bytes encryptedData = EncryptAes128Cbc(data, CopyKey, initializationVector);
    copy.insert(copy.end(), encryptedData.begin(), encryptedData.end());

    Sha1Digest hmac = ComputeHmacSha1(hmacKey, copy);
    std::copy(hmac.begin(), hmac.end(), copy.begin() + CdbSdCopyHmacOffset);
    return copy;
}

}
