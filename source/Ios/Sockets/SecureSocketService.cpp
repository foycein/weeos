#include "Ios/Sockets/SecureSocketService.h"

#include "Ios/Sockets/CertificateCheck.h"

#include <algorithm>
#include <vector>

namespace Ios
{

namespace
{

// The contexts are numbered from 1. The number 0 is no context.
constexpr SecureContextId FirstContextId = 1;

bool AsksFor(std::uint32_t verifyFlags, std::uint32_t check)
{
    return (verifyFlags & check) != 0;
}

bool AreAllInDate(const std::vector<Formats::Bytes>& certificates)
{
    return std::ranges::all_of(certificates, [](const Formats::Bytes& certificate) { return IsInDate(certificate); });
}

}

SecureSocketService::SecureSocketService(SocketService& socketService)
    : sockets(socketService)
{
}

SecureSocketService::~SecureSocketService() = default;

SecureSocketService::Context* SecureSocketService::FindOpenContext(SecureContextId context)
{
    bool isNumberOfAContext = context >= FirstContextId && context <= static_cast<SecureContextId>(SecureContextCount);
    if (!isNumberOfAContext)
        return nullptr;

    return contexts[context - FirstContextId].get();
}

Result<SecureContextId> SecureSocketService::NewContext(std::uint32_t verifyFlags, std::string_view commonName)
{
    if (commonName.size() > MaximumCommonNameLength)
        return IosError::SecureLibraryFailure;

    std::lock_guard<std::mutex> lock(mutex);
    auto freeSlot = std::find(contexts.begin(), contexts.end(), nullptr);
    if (freeSlot == contexts.end())
        return IosError::SecureLibraryFailure;

    *freeSlot = std::make_unique<Context>();
    (*freeSlot)->verifyFlags = verifyFlags;
    (*freeSlot)->commonName = std::string(commonName);
    return static_cast<SecureContextId>(freeSlot - contexts.begin()) + FirstContextId;
}

Result<void> SecureSocketService::Connect(SecureContextId contextId, SocketId socket)
{
    std::lock_guard<std::mutex> lock(mutex);
    Context* context = FindOpenContext(contextId);
    if (context == nullptr)
        return IosError::InvalidSecureContext;

    context->connection = std::make_unique<TlsConnection>(sockets, socket, context->commonName);
    context->state = ContextState::Connected;
    return Result<void>();
}

// The checks in the order of the verify flags. The built-in roots are not carried, so a context without a root of its own
// trusts no server (SecureSocketService.h).
Result<void> SecureSocketService::CheckServerCertificates(const Context& context) const
{
    std::vector<Formats::Bytes> certificates = context.connection->ServerCertificates();
    if (certificates.empty())
        return IosError::SecureConnectionFailed;

    if (AsksFor(context.verifyFlags, VerifyCommonName) && CommonNameOf(certificates.front()) != context.commonName)
        return IosError::CommonNameMismatch;

    if (AsksFor(context.verifyFlags, VerifyRootCa) && !IsTrustedByRoot(certificates, context.rootCertificate))
        return IosError::RootCertificateNotTrusted;

    if (AsksFor(context.verifyFlags, VerifyChain) && !IsChainUnbroken(certificates, context.rootCertificate))
        return IosError::CertificateChainBroken;

    if (AsksFor(context.verifyFlags, VerifyDate) && !AreAllInDate(certificates))
        return IosError::CertificateOutOfDate;

    return Result<void>();
}

Result<void> SecureSocketService::SetTheConnectionUp(Context& context)
{
    if (context.state == ContextState::Established)
        return Result<void>();

    if (context.state == ContextState::New)
        return IosError::SecureLibraryFailure;

    if (context.state == ContextState::Failed)
        return IosError::SecureConnectionFailed;

    if (!context.connection->Handshake())
    {
        context.state = ContextState::Failed;
        return IosError::SecureConnectionFailed;
    }

    Result<void> checked = CheckServerCertificates(context);
    if (checked.Failed())
    {
        context.state = ContextState::Failed;
        return checked;
    }

    context.state = ContextState::Established;
    return Result<void>();
}

// The network calls run without the lock, so that the other contexts are not held up.
Result<void> SecureSocketService::Handshake(SecureContextId contextId)
{
    Context* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex);
        context = FindOpenContext(contextId);
    }
    if (context == nullptr)
        return IosError::InvalidSecureContext;

    return SetTheConnectionUp(*context);
}

Result<Formats::Bytes> SecureSocketService::Read(SecureContextId contextId, std::size_t maximumLength)
{
    Context* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex);
        context = FindOpenContext(contextId);
    }
    if (context == nullptr)
        return IosError::InvalidSecureContext;

    Result<void> setUp = SetTheConnectionUp(*context);
    if (setUp.Failed())
        return setUp.Error();

    std::optional<Formats::Bytes> data = context->connection->Read(maximumLength);
    if (!data)
        return IosError::SecureConnectionFailed;

    return *data;
}

Result<std::size_t> SecureSocketService::Write(SecureContextId contextId, Formats::ByteView data)
{
    Context* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex);
        context = FindOpenContext(contextId);
    }
    if (context == nullptr)
        return IosError::InvalidSecureContext;

    Result<void> setUp = SetTheConnectionUp(*context);
    if (setUp.Failed())
        return setUp.Error();

    if (!context->connection->Write(data))
        return IosError::SecureConnectionFailed;

    return data.size();
}

Result<void> SecureSocketService::Shutdown(SecureContextId contextId)
{
    std::unique_ptr<Context> context;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (FindOpenContext(contextId) == nullptr)
            return IosError::InvalidSecureContext;

        context = std::move(contexts[contextId - FirstContextId]);
    }

    if (context->state == ContextState::Established)
        context->connection->SayGoodbye();

    bool couldSayGoodbye = context->state == ContextState::Connected || context->state == ContextState::Established;
    if (!couldSayGoodbye)
        return IosError::SecureLibraryFailure;

    return Result<void>();
}

Result<void> SecureSocketService::SetRootCa(SecureContextId contextId, Formats::ByteView certificate)
{
    std::lock_guard<std::mutex> lock(mutex);
    Context* context = FindOpenContext(contextId);
    if (context == nullptr)
        return IosError::InvalidSecureContext;

    if (certificate.size() > MaximumRootCertificateSize)
        return IosError::SecureLibraryFailure;

    context->rootCertificate.assign(certificate.begin(), certificate.end());
    return Result<void>();
}

Result<void> SecureSocketService::SetClientCertificate(SecureContextId contextId, Formats::ByteView certificate, Formats::ByteView privateKey)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (FindOpenContext(contextId) == nullptr)
        return IosError::InvalidSecureContext;

    if (certificate.size() > MaximumClientCertificateSize || privateKey.size() > MaximumPrivateKeySize)
        return IosError::SecureLibraryFailure;

    return Result<void>();
}

Result<void> SecureSocketService::SetBuiltinRootCa(SecureContextId contextId, std::uint32_t index)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (FindOpenContext(contextId) == nullptr)
        return IosError::InvalidSecureContext;

    if (index >= BuiltinRootCertificateCount)
        return IosError::BuiltinRootCertificateNotFound;

    return Result<void>();
}

Result<void> SecureSocketService::SetBuiltinClientCertificate(SecureContextId contextId, std::uint32_t index)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (FindOpenContext(contextId) == nullptr)
        return IosError::InvalidSecureContext;

    if (index >= BuiltinClientCertificateCount)
        return IosError::BuiltinClientCertificateNotFound;

    return Result<void>();
}

}
