#pragma once

#include "Formats/Bytes.h"
#include "Ios/Sockets/SocketService.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Ios
{

// One TLS connection of Windows (Schannel), the client side, over a connected socket of the socket service. The secure sockets use it
// for the handshake, the reads and the writes. It checks no certificate itself: the secure sockets check the server's certificates
// the way the console does (CertificateCheck.h).
class TlsConnection
{
public:
    // The server name is told to the server, so that it can choose its certificate.
    TlsConnection(SocketService& sockets, SocketId socket, const std::string& serverName);
    ~TlsConnection();

    TlsConnection(const TlsConnection&) = delete;
    TlsConnection& operator=(const TlsConnection&) = delete;

    // Sets the connection up. False when the socket fails or the other side does not set up TLS.
    bool Handshake();

    // The certificates the server sent, its own first. Empty before the handshake.
    std::vector<Formats::Bytes> ServerCertificates() const;

    // Up to maximumLength bytes that the server sent. Empty when the server closed the connection, nothing when it failed.
    std::optional<Formats::Bytes> Read(std::size_t maximumLength);

    // Sends all the bytes. False when the connection failed.
    bool Write(Formats::ByteView data);

    // Tells the server that the connection ends.
    void SayGoodbye();

private:
    // The handshake and the records with the handles and buffers of Windows, kept out of this header.
    class Session;

    std::unique_ptr<Session> session;
};

}
