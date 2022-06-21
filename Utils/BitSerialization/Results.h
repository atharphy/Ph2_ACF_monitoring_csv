#ifndef BITSERIALIZATION_RESULTS_H
#define BITSERIALIZATION_RESULTS_H

#include "Errors.h"
#include <optional>

namespace BitSerialization
{
struct ResultBase
{
};

template <class T, class Error = VoidError>
class ParseResult : public ResultBase
{
    static_assert(!std::is_base_of_v<ResultBase, T>);

    struct _Data
    {
        T      value;
        size_t size;
    };

  public:
    using value_type = T;
    using error_type = Error;

    ParseResult() {}

    // ParseResult(const ParseResult&) = default;

    // ParseResult(ParseResult&&) = default;

    ParseResult(T&& value, size_t size) : _storage(_Data{std::move(value), size}) {}

    ParseResult(const T& value, size_t size) : _storage(_Data{value, size}) {}

    ParseResult(Error&& error) : _storage(std::move(error)) {}

    operator bool() const { return !std::holds_alternative<Error>(_storage); }

    T&       value() { return std::get<_Data>(_storage).value; }
    const T& value() const { return std::get<_Data>(_storage).value; }

    size_t&       size() { return std::get<_Data>(_storage).size; }
    const size_t& size() const { return std::get<_Data>(_storage).size; }

    Error&       error() { return std::get<Error>(_storage); }
    const Error& error() const { return std::get<Error>(_storage); }

  private:
    std::variant<_Data, Error> _storage;
};

template <class Error = VoidError>
struct SerializeResult
{
    using error_type = Error;

    SerializeResult() {}

    SerializeResult(Error&& error) : _storage(std::move(error)) {}

    operator bool() const { return !bool(_storage); }

    Error&       error() { return _storage.value(); }
    const Error& error() const { return _storage.value(); }

  private:
    std::optional<Error> _storage;
};

} // namespace BitSerialization

#endif
