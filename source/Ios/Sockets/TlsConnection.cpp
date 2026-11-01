#include "Ios/Sockets/TlsConnection.h"

#define SECURITY_WIN32
#include <windows.h>
#include <security.h>
#include <schannel.h>

#include <algorithm>

namespace Ios
{

namespace
{

// A TLS record holds at most 16 KiB of data, with its header and its trailer around it.
constexpr std::size_t LargestRecordData = 16 * 1024;
constexpr std::size_t RoomForHeaderAndTrailer = 512;
constexpr std::size_t ReceiveChunkSize = LargestRecordData + RoomForHeaderAndTrailer;

constexpr DWORD HandshakeRequests = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY |
    ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION;

// DecryptMessage and EncryptMessage take four buffers: the record, and room for what they give back.
constexpr unsigned long MessageBufferCount = 4;
constexpr std::size_t HeaderBufferIndex = 0;
constexpr std::size_t DataBufferIndex = 1;
constexpr std::size_t TrailerBufferIndex = 2;

// A handshake step takes what the server sent, and Windows says in the second buffer which of those bytes it did not use.
constexpr unsigned long HandshakeInputBufferCount = 2;
constexpr std::size_t UnusedBytesBufferIndex = 1;

enum class DecryptResult
{
    Decrypted,
    NeedsMoreBytes,
    Ended,
    Failed,
};

Formats::ByteView BytesOf(const SecBuffer& buffer)
{
    return Formats::ByteView(static_cast<const std::uint8_t*>(buffer.pvBuffer), buffer.cbBuffer);
}

void Append(Formats::Bytes& bytes, Formats::ByteView more)
{
    bytes.insert(bytes.end(), more.begin(), more.end());
}

// The last bytes of a buffer, which Windows did not use.
Formats::Bytes LastBytes(const Formats::Bytes& bytes, std::size_t count)
{
    return Formats::Bytes(bytes.end() - static_cast<std::ptrdiff_t>(count), bytes.end());
}

}

class TlsConnection::Session
{
public:
    Session(SocketService& socketService, SocketId socketToUse, const std::string& nameOfServer)
        : sockets(socketService)
        , socket(socketToUse)
        , serverName(nameOfServer.begin(), nameOfServer.end())
    {
    }

    ~Session()
    {
        if (hasContext)
            DeleteSecurityContext(&context);

        if (hasCredentials)
            FreeCredentialsHandle(&credentials);
    }

    bool Handshake()
    {
        hasCredentials = AcquireCredentials();
        if (!hasCredentials)
            return false;

        SECURITY_STATUS status = TakeHandshakeStep(nullptr);
        while (status == SEC_I_CONTINUE_NEEDED || status == SEC_E_INCOMPLETE_MESSAGE)
        {
            bool needsBytesOfTheServer = received.empty() || status == SEC_E_INCOMPLETE_MESSAGE;
            if (needsBytesOfTheServer && !ReceiveMore())
                return false;

            status = TakeHandshakeStep(&context);
        }

        if (status != SEC_E_OK)
            return false;

        return QueryContextAttributesW(&context, SECPKG_ATTR_STREAM_SIZES, &sizes) == SEC_E_OK;
    }

    std::vector<Formats::Bytes> ServerCertificates()
    {
        if (!hasContext)
            return {};

        const CERT_CONTEXT* serverCertificate = nullptr;
        if (QueryContextAttributesW(&context, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &serverCertificate) != SEC_E_OK)
            return {};

        Formats::ByteView serverBytes(serverCertificate->pbCertEncoded, serverCertificate->cbCertEncoded);
        std::vector<Formats::Bytes> certificates = {Formats::Bytes(serverBytes.begin(), serverBytes.end())};

        // The other certificates that the server sent are in the store of its certificate.
        const CERT_CONTEXT* other = CertEnumCertificatesInStore(serverCertificate->hCertStore, nullptr);
        while (other != nullptr)
        {
            Formats::ByteView otherBytes(other->pbCertEncoded, other->cbCertEncoded);
            if (!std::ranges::equal(otherBytes, serverBytes))
                certificates.emplace_back(otherBytes.begin(), otherBytes.end());

            other = CertEnumCertificatesInStore(serverCertificate->hCertStore, other);
        }

        CertFreeCertificateContext(serverCertificate);
        return certificates;
    }

    std::optional<Formats::Bytes> Read(std::size_t maximumLength)
    {
        while (plainText.empty())
        {
            DecryptResult result = DecryptReceivedBytes();
            if (result == DecryptResult::Failed)
                return std::nullopt;

            if (result == DecryptResult::Ended)
                return Formats::Bytes();

            if (result == DecryptResult::Decrypted)
                continue;

            Result<Formats::Bytes> more = sockets.ReceiveFrom(socket, ReceiveChunkSize);
            if (more.Failed())
                return std::nullopt;

            // The server closed the socket without saying goodbye.
            if (more.Value().empty())
                return Formats::Bytes();

            Append(received, more.Value());
        }

        std::size_t length = std::min(maximumLength, plainText.size());
        Formats::Bytes data(plainText.begin(), plainText.begin() + static_cast<std::ptrdiff_t>(length));
        plainText.erase(plainText.begin(), plainText.begin() + static_cast<std::ptrdiff_t>(length));
        return data;
    }

    // The data goes in records of at most the size that the connection allows, each with its header and its trailer.
    bool Write(Formats::ByteView data)
    {
        while (!data.empty())
        {
            std::size_t length = std::min<std::size_t>(data.size(), sizes.cbMaximumMessage);
            Formats::Bytes record(sizes.cbHeader + length + sizes.cbTrailer);
            std::copy_n(data.begin(), length, record.begin() + sizes.cbHeader);

            SecBuffer buffers[MessageBufferCount] = {
                {sizes.cbHeader, SECBUFFER_STREAM_HEADER, record.data()},
                {static_cast<unsigned long>(length), SECBUFFER_DATA, record.data() + sizes.cbHeader},
                {sizes.cbTrailer, SECBUFFER_STREAM_TRAILER, record.data() + sizes.cbHeader + length},
                {0, SECBUFFER_EMPTY, nullptr},
            };
            SecBufferDesc message = {SECBUFFER_VERSION, MessageBufferCount, buffers};
            if (EncryptMessage(&context, 0, &message, 0) != SEC_E_OK)
                return false;

            std::size_t headerLength = buffers[HeaderBufferIndex].cbBuffer;
            std::size_t dataLength = buffers[DataBufferIndex].cbBuffer;
            std::size_t trailerLength = buffers[TrailerBufferIndex].cbBuffer;
            std::size_t recordLength = headerLength + dataLength + trailerLength;
            if (!SendAll(Formats::ByteView(record).first(recordLength)))
                return false;

            data = data.subspan(length);
        }
        return true;
    }

    void SayGoodbye()
    {
        if (!hasContext)
            return;

        DWORD shutdownToken = SCHANNEL_SHUTDOWN;
        SecBuffer controlBuffer = {sizeof(shutdownToken), SECBUFFER_TOKEN, &shutdownToken};
        SecBufferDesc control = {SECBUFFER_VERSION, 1, &controlBuffer};
        if (ApplyControlToken(&context, &control) != SEC_E_OK)
            return;

        SecBuffer token = {0, SECBUFFER_TOKEN, nullptr};
        SecBufferDesc output = {SECBUFFER_VERSION, 1, &token};
        unsigned long answeredFlags = 0;
        InitializeSecurityContextW(&credentials, &context, nullptr, HandshakeRequests, 0, 0, nullptr, 0, nullptr, &output, &answeredFlags,
            nullptr);
        SendToken(token);
    }

private:
    // The console checks the certificates of the server itself, so Windows is told not to.
    bool AcquireCredentials()
    {
        SCHANNEL_CRED settings = {};
        settings.dwVersion = SCHANNEL_CRED_VERSION;
        settings.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

        SECURITY_STATUS status = AcquireCredentialsHandleW(nullptr, const_cast<wchar_t*>(UNISP_NAME_W), SECPKG_CRED_OUTBOUND, nullptr,
            &settings, nullptr, nullptr, &credentials, nullptr);
        return status == SEC_E_OK;
    }

    // One step of the handshake: Windows reads what the server sent and makes what goes back. The first step has no context yet
    // and nothing of the server. The bytes of the server that the step did not use stay for the next one.
    SECURITY_STATUS TakeHandshakeStep(CtxtHandle* existingContext)
    {
        SecBuffer inputBuffers[HandshakeInputBufferCount] = {
            {static_cast<unsigned long>(received.size()), SECBUFFER_TOKEN, received.data()},
            {0, SECBUFFER_EMPTY, nullptr},
        };
        SecBufferDesc input = {SECBUFFER_VERSION, HandshakeInputBufferCount, inputBuffers};
        SecBufferDesc* serverBytes = existingContext == nullptr ? nullptr : &input;

        SecBuffer token = {0, SECBUFFER_TOKEN, nullptr};
        SecBufferDesc output = {SECBUFFER_VERSION, 1, &token};
        unsigned long answeredFlags = 0;
        wchar_t* targetName = serverName.empty() ? nullptr : serverName.data();

        SECURITY_STATUS status = InitializeSecurityContextW(&credentials, existingContext, targetName, HandshakeRequests, 0, 0, serverBytes,
            0, &context, &output, &answeredFlags, nullptr);
        if (existingContext == nullptr && status == SEC_I_CONTINUE_NEEDED)
            hasContext = true;

        if (status == SEC_E_INCOMPLETE_MESSAGE)
            return status;

        if (!SendToken(token))
            return SEC_E_INTERNAL_ERROR;

        if (existingContext != nullptr)
            KeepUnusedBytes(inputBuffers[UnusedBytesBufferIndex]);

        return status;
    }

    // Windows made the token that goes to the server. It is sent and given back to Windows.
    bool SendToken(SecBuffer& token)
    {
        if (token.pvBuffer == nullptr)
            return true;

        bool isSent = SendAll(BytesOf(token));
        FreeContextBuffer(token.pvBuffer);
        return isSent;
    }

    void KeepUnusedBytes(const SecBuffer& extraBuffer)
    {
        if (extraBuffer.BufferType == SECBUFFER_EXTRA)
            received = LastBytes(received, extraBuffer.cbBuffer);
        else
            received.clear();
    }

    // One record of the received bytes is decrypted, and its data waits to be read.
    DecryptResult DecryptReceivedBytes()
    {
        if (received.empty())
            return DecryptResult::NeedsMoreBytes;

        SecBuffer buffers[MessageBufferCount] = {
            {static_cast<unsigned long>(received.size()), SECBUFFER_DATA, received.data()},
            {0, SECBUFFER_EMPTY, nullptr},
            {0, SECBUFFER_EMPTY, nullptr},
            {0, SECBUFFER_EMPTY, nullptr},
        };
        SecBufferDesc message = {SECBUFFER_VERSION, MessageBufferCount, buffers};

        SECURITY_STATUS status = DecryptMessage(&context, &message, 0, nullptr);
        if (status == SEC_E_INCOMPLETE_MESSAGE)
            return DecryptResult::NeedsMoreBytes;

        if (status == SEC_I_CONTEXT_EXPIRED)
            return DecryptResult::Ended;

        if (status != SEC_E_OK)
            return DecryptResult::Failed;

        Formats::Bytes unused;
        for (const SecBuffer& buffer : buffers)
        {
            if (buffer.BufferType == SECBUFFER_DATA)
                Append(plainText, BytesOf(buffer));

            if (buffer.BufferType == SECBUFFER_EXTRA)
                unused = LastBytes(received, buffer.cbBuffer);
        }
        received = std::move(unused);
        return DecryptResult::Decrypted;
    }

    bool SendAll(Formats::ByteView data)
    {
        while (!data.empty())
        {
            Result<std::size_t> sent = sockets.SendTo(socket, data, nullptr);
            if (sent.Failed())
                return false;

            data = data.subspan(sent.Value());
        }
        return true;
    }

    bool ReceiveMore()
    {
        Result<Formats::Bytes> more = sockets.ReceiveFrom(socket, ReceiveChunkSize);
        if (more.Failed() || more.Value().empty())
            return false;

        Append(received, more.Value());
        return true;
    }

    SocketService& sockets;
    SocketId socket;
    std::wstring serverName;

    CredHandle credentials = {};
    CtxtHandle context = {};
    bool hasCredentials = false;
    bool hasContext = false;
    SecPkgContext_StreamSizes sizes = {};

    // Bytes of the server that are not decrypted yet, and decrypted bytes that were not read yet.
    Formats::Bytes received;
    Formats::Bytes plainText;
};

TlsConnection::TlsConnection(SocketService& sockets, SocketId socket, const std::string& serverName)
    : session(std::make_unique<Session>(sockets, socket, serverName))
{
}

TlsConnection::~TlsConnection() = default;

bool TlsConnection::Handshake()
{
    return session->Handshake();
}

std::vector<Formats::Bytes> TlsConnection::ServerCertificates() const
{
    return session->ServerCertificates();
}

std::optional<Formats::Bytes> TlsConnection::Read(std::size_t maximumLength)
{
    return session->Read(maximumLength);
}

bool TlsConnection::Write(Formats::ByteView data)
{
    return session->Write(data);
}

void TlsConnection::SayGoodbye()
{
    session->SayGoodbye();
}

}
