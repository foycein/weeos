#include "Ios/Sockets/SecureSocketService.h"

#include <algorithm>

namespace Ios
{

namespace
{

// The contexts are numbered from 1. The number 0 is no context.
constexpr SecureContextId FirstContextId = 1;

}

SecureSocketService::ContextState* SecureSocketService::FindOpenContext(SecureContextId context)
{
    bool isNumberOfAContext = context >= FirstContextId && context <= static_cast<SecureContextId>(SecureContextCount);
    if (!isNumberOfAContext)
        return nullptr;

    ContextState& state = states[context - FirstContextId];
    if (state == ContextState::Free)
        return nullptr;

    return &state;
}

Result<SecureContextId> SecureSocketService::NewContext(std::uint32_t, std::string_view commonName)
{
    if (commonName.size() > MaximumCommonNameLength)
        return IosError::SecureLibraryFailure;

    std::lock_guard<std::mutex> lock(mutex);
    auto freeContext = std::find(states.begin(), states.end(), ContextState::Free);
    if (freeContext == states.end())
        return IosError::SecureLibraryFailure;

    *freeContext = ContextState::New;
    return static_cast<SecureContextId>(freeContext - states.begin()) + FirstContextId;
}

Result<void> SecureSocketService::Connect(SecureContextId context, SocketId)
{
    std::lock_guard<std::mutex> lock(mutex);
    ContextState* state = FindOpenContext(context);
    if (state == nullptr)
        return IosError::InvalidSecureContext;

    *state = ContextState::Connected;
    return Result<void>();
}

IosError SecureSocketService::TryToSetTheConnectionUp(SecureContextId context)
{
    std::lock_guard<std::mutex> lock(mutex);
    ContextState* state = FindOpenContext(context);
    if (state == nullptr)
        return IosError::InvalidSecureContext;

    if (*state == ContextState::New)
        return IosError::SecureLibraryFailure;

    *state = ContextState::Failed;
    return IosError::SecureConnectionFailed;
}

Result<void> SecureSocketService::Handshake(SecureContextId context)
{
    return TryToSetTheConnectionUp(context);
}

Result<Formats::Bytes> SecureSocketService::Read(SecureContextId context, std::size_t)
{
    return TryToSetTheConnectionUp(context);
}

Result<std::size_t> SecureSocketService::Write(SecureContextId context, Formats::ByteView)
{
    return TryToSetTheConnectionUp(context);
}

Result<void> SecureSocketService::Shutdown(SecureContextId context)
{
    std::lock_guard<std::mutex> lock(mutex);
    ContextState* state = FindOpenContext(context);
    if (state == nullptr)
        return IosError::InvalidSecureContext;

    bool couldSayGoodbye = *state == ContextState::Connected;
    *state = ContextState::Free;
    if (!couldSayGoodbye)
        return IosError::SecureLibraryFailure;

    return Result<void>();
}

Result<void> SecureSocketService::SetRootCa(SecureContextId context, Formats::ByteView certificate)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (FindOpenContext(context) == nullptr)
        return IosError::InvalidSecureContext;

    if (certificate.size() > MaximumRootCertificateSize)
        return IosError::SecureLibraryFailure;

    return Result<void>();
}

Result<void> SecureSocketService::SetClientCertificate(SecureContextId context, Formats::ByteView certificate, Formats::ByteView privateKey)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (FindOpenContext(context) == nullptr)
        return IosError::InvalidSecureContext;

    if (certificate.size() > MaximumClientCertificateSize || privateKey.size() > MaximumPrivateKeySize)
        return IosError::SecureLibraryFailure;

    return Result<void>();
}

Result<void> SecureSocketService::SetBuiltinRootCa(SecureContextId context, std::uint32_t index)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (FindOpenContext(context) == nullptr)
        return IosError::InvalidSecureContext;

    if (index >= BuiltinRootCertificateCount)
        return IosError::BuiltinRootCertificateNotFound;

    return Result<void>();
}

Result<void> SecureSocketService::SetBuiltinClientCertificate(SecureContextId context, std::uint32_t index)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (FindOpenContext(context) == nullptr)
        return IosError::InvalidSecureContext;

    if (index >= BuiltinClientCertificateCount)
        return IosError::BuiltinClientCertificateNotFound;

    return Result<void>();
}

}
