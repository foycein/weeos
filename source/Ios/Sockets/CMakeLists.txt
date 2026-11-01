target_sources(Ios PRIVATE
    CertificateCheck.cpp
    InternetAddress.cpp
    SecureSocketService.cpp
    SocketService.cpp
    TlsConnection.cpp
)
target_link_libraries(Ios PUBLIC ws2_32 secur32 crypt32)
