#pragma once

#include "Ios/IosError.h"

#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace Ios
{

// What a call gives back: the value it made, or the reason it failed.
//
//     Result<OpenFile> opened = fileSystem.Open(path, OpenMode::Read, caller);
//     if (opened.Failed())
//         return opened.Error();
//
// Asking for the value of a failed result, or the error of a successful one, is a mistake in the program and throws.
template <typename ValueType>
class Result
{
public:
    Result(ValueType value)
        : outcome(std::move(value))
    {
    }

    Result(IosError error)
        : outcome(error)
    {
    }

    bool Succeeded() const
    {
        return std::holds_alternative<ValueType>(outcome);
    }

    bool Failed() const
    {
        return !Succeeded();
    }

    ValueType& Value()
    {
        RequireSuccess();
        return std::get<ValueType>(outcome);
    }

    const ValueType& Value() const
    {
        RequireSuccess();
        return std::get<ValueType>(outcome);
    }

    IosError Error() const
    {
        if (Succeeded())
            throw std::logic_error("Result::Error was called on a successful result");

        return std::get<IosError>(outcome);
    }

private:
    void RequireSuccess() const
    {
        if (Failed())
            throw std::logic_error("Result::Value was called on a failed result");
    }

    std::variant<ValueType, IosError> outcome;
};

// The result of a call that gives back nothing. A default result is a success.
template <>
class Result<void>
{
public:
    Result() = default;

    Result(IosError failure)
        : error(failure)
    {
    }

    bool Succeeded() const
    {
        return !error.has_value();
    }

    bool Failed() const
    {
        return error.has_value();
    }

    IosError Error() const
    {
        if (Succeeded())
            throw std::logic_error("Result::Error was called on a successful result");

        return *error;
    }

private:
    std::optional<IosError> error;
};

}
