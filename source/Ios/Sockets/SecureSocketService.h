#pragma once

#include "Formats/Bytes.h"
#include "Ios/Result.h"
#include "Ios/Sockets/SocketTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
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

// The secure sockets (SSL) of IOS, in place of /dev/net/ssl.
//
// The module makes TLS connections over sockets of the socket stack. A connection starts with a context (NewContext), takes a
// socket (Connect), and is then set up (Handshake) before it reads and writes. Here the socket stack never runs, so no socket
// exists and the handshake can only fail, the way the module fails when the socket under it fails (docs/audit/Ios.md).
// Everything that is only a setting of a context works.
//
// The calls that no program of the System Menu or the channels makes are not rebuilt: the default and removal of certificates,
// the handshake with a certificate in the answer, switching the verification off, and the debug calls.
//
// All calls are thread safe.
class SecureSocketService
{
public:
    // Opens a context. verifyFlags says which checks of the certificate of the server the module makes; a handshake never gets
    // as far here, so they change nothing. The common name is the name that the certificate of the server has to carry.
    // SecureLibraryFailure when all four contexts are in use, or the name is longer than MaximumCommonNameLength.
    Result<SecureContextId> NewContext(std::uint32_t verifyFlags, std::string_view commonName);

    // Gives the context the socket to talk over. The number of the socket is not looked at, as in the original.
    // InvalidSecureContext when the number is not an open context.
    Result<void> Connect(SecureContextId context, SocketId socket);

    // Sets the connection up. The socket under it fails, so it fails: SecureConnectionFailed.
    // SecureLibraryFailure when the context has no socket yet.
    Result<void> Handshake(SecureContextId context);

    // Both fail like Handshake, because they start the handshake when it has not been done.
    Result<Formats::Bytes> Read(SecureContextId context, std::size_t maximumLength);
    Result<std::size_t> Write(SecureContextId context, Formats::ByteView data);

    // Closes the context, which is free to be opened again. Success when no handshake was tried, SecureLibraryFailure
    // when it was (the goodbye cannot be sent) and when the context has no socket.
    Result<void> Shutdown(SecureContextId context);

    // The settings of a context. The certificates are only kept for the handshake and are not looked at here.
    // SecureLibraryFailure when a certificate or a key is bigger than the module takes.
    Result<void> SetRootCa(SecureContextId context, Formats::ByteView certificate);
    Result<void> SetClientCertificate(SecureContextId context, Formats::ByteView certificate, Formats::ByteView privateKey);

    // BuiltinRootCertificateNotFound and BuiltinClientCertificateNotFound when the number is too big.
    Result<void> SetBuiltinRootCa(SecureContextId context, std::uint32_t index);
    Result<void> SetBuiltinClientCertificate(SecureContextId context, std::uint32_t index);

private:
    enum class ContextState
    {
        Free,

        // Opened, but it has no socket yet.
        New,

        // It has a socket and no handshake was tried.
        Connected,

        // A handshake, a read or a write was tried and failed.
        Failed,
    };

    // The state of the context with the number, or nothing when the number is not an open context. The lock has to be held.
    ContextState* FindOpenContext(SecureContextId context);

    // A handshake, a read and a write all start by setting the connection up, and the socket under it fails.
    IosError TryToSetTheConnectionUp(SecureContextId context);

    mutable std::mutex mutex;
    std::array<ContextState, SecureContextCount> states{};
};

}
