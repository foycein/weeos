#pragma once

#include "Formats/Bytes.h"
#include "Ios/Result.h"
#include "Ios/Sockets/SocketService.h"
#include "Ios/Sockets/SocketTypes.h"
#include "Ios/Sockets/TlsConnection.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace Ios
{

// The number of a secure connection. The module has four, numbered 1 to 4.
using SecureContextId = std::int32_t;

constexpr std::size_t SecureContextCount = 4;

// What the module accepts. A text is shorter than the field of 0x100 bytes that carries it.
constexpr std::size_t MaximumCommonNameLength = 0xFF;
constexpr std::size_t MaximumRootCertificateSize = 0x800;
constexpr std::size_t MaximumClientCertificateSize = 0x800;
constexpr std::size_t MaximumPrivateKeySize = 0x400;

// The module carries four root certificates and one client certificate. A program picks one by its number.
constexpr std::uint32_t BuiltinRootCertificateCount = 4;
constexpr std::uint32_t BuiltinClientCertificateCount = 1;

// The checks of the server's certificate that a program asks for when it opens a context (the verify flags of the SDK's SSLNew).
constexpr std::uint32_t VerifyCommonName = 0x01;
constexpr std::uint32_t VerifyRootCa = 0x02;
constexpr std::uint32_t VerifyChain = 0x04;
constexpr std::uint32_t VerifyDate = 0x08;

// The secure sockets (SSL) of IOS, in place of /dev/net/ssl.
//
// The module makes TLS connections over sockets of the socket stack. A connection starts with a context (NewContext), takes a
// socket (Connect), and is then set up (Handshake) before it reads and writes. The TLS is the one of Windows (Schannel, TlsConnection);
// the checks of the server's certificate are the console's, with its error numbers (docs/audit/Ios.md).
//
// The root certificates built into the module are Nintendo's; WeeOS reaches no Nintendo server, so it does not carry them, and a
// context that trusts only a built-in root trusts no server. A client certificate is kept but not shown to a server.
//
// The calls that no program of the System Menu or the channels makes are not rebuilt: the default and removal of certificates,
// the handshake with a certificate in the answer, switching the verification off, and the debug calls.
//
// All calls are thread safe; one context is used by one program at a time.
class SecureSocketService
{
public:
    // The connections run over the sockets of the socket service, which must outlive this object.
    explicit SecureSocketService(SocketService& socketService);
    ~SecureSocketService();

    // Opens a context. verifyFlags says which checks of the certificate of the server the handshake makes (the Verify constants).
    // The common name is the name that the certificate of the server has to carry.
    // SecureLibraryFailure when all four contexts are in use, or the name is longer than MaximumCommonNameLength.
    Result<SecureContextId> NewContext(std::uint32_t verifyFlags, std::string_view commonName);

    // Gives the context the socket to talk over. The number of the socket is not looked at, as in the original.
    // InvalidSecureContext when the number is not an open context.
    Result<void> Connect(SecureContextId context, SocketId socket);

    // Sets the connection up and checks the server's certificate. SecureConnectionFailed when the socket fails or the server does not
    // set up TLS; CommonNameMismatch, RootCertificateNotTrusted, CertificateChainBroken or CertificateOutOfDate when a check fails.
    // SecureLibraryFailure when the context has no socket yet. A context whose handshake failed fails every later call.
    Result<void> Handshake(SecureContextId context);

    // Both set the connection up first when that has not been done. An empty read means that the server closed the connection.
    Result<Formats::Bytes> Read(SecureContextId context, std::size_t maximumLength);
    Result<std::size_t> Write(SecureContextId context, Formats::ByteView data);

    // Says goodbye to the server and closes the context, which is free to be opened again. Success when no handshake was tried or
    // the connection was set up; SecureLibraryFailure when the handshake failed and when the context has no socket.
    Result<void> Shutdown(SecureContextId context);

    // The settings of a context. SecureLibraryFailure when a certificate or a key is bigger than the module takes.
    Result<void> SetRootCa(SecureContextId context, Formats::ByteView certificate);
    Result<void> SetClientCertificate(SecureContextId context, Formats::ByteView certificate, Formats::ByteView privateKey);

    // BuiltinRootCertificateNotFound and BuiltinClientCertificateNotFound when the number is too big.
    Result<void> SetBuiltinRootCa(SecureContextId context, std::uint32_t index);
    Result<void> SetBuiltinClientCertificate(SecureContextId context, std::uint32_t index);

private:
    enum class ContextState
    {
        // Opened, but it has no socket yet.
        New,

        // It has a socket and no handshake was tried.
        Connected,

        // The connection is set up.
        Established,

        // A handshake was tried and failed.
        Failed,
    };

    struct Context
    {
        ContextState state = ContextState::New;
        std::uint32_t verifyFlags = 0;
        std::string commonName;
        Formats::Bytes rootCertificate;
        std::unique_ptr<TlsConnection> connection;
    };

    // The context with the number, or nothing when the number is not an open context.
    Context* FindOpenContext(SecureContextId context);

    // A handshake, a read and a write all start by setting the connection up.
    Result<void> SetTheConnectionUp(Context& context);
    Result<void> CheckServerCertificates(const Context& context) const;

    SocketService& sockets;
    mutable std::mutex mutex;
    std::array<std::unique_ptr<Context>, SecureContextCount> contexts;
};

}
