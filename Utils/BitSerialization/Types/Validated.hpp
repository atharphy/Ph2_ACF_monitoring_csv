#ifndef BITSERIALIZATION__TYPES__VALIDATED_HPP
#define BITSERIALIZATION__TYPES__VALIDATED_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class Type, auto Predicate>
struct Validated {
    using value_type = value_type_t<Type>;

    using _ValueError = ValueError<"Validated", value_type>;

    using ParseError = ErrorVariant<
        _ValueError, 
        parse_error_t<Type>
    >;

    template <class T, class U=Void>
    static ParseResult<value_type, ParseError> 
    parse(const BitView<T>& bits, const U& parent={}) 
    {
        auto result = Type::parse(bits, parent);
        if (!result)
            return {std::move(result.error())};
        if (!Predicate(parent, result.value()))
            return {_ValueError{std::move(result.value())}};
        else
            return {std::move(result.value()), result.size()};
    }
    
    using SerializeError = ErrorVariant<
        _ValueError, 
        serialize_error_t<Type>
    >;

    template <class T, class U=Void>
    static SerializeResult<SerializeError> 
    serialize(value_type& value, BitVector<T>& bits, const U& parent={}) {
        if (!Predicate(parent, value))
            return {_ValueError{value}};
        auto result = Type::serialize(value, bits, parent);
        if (!result)
            return {std::move(result.error())};
        else
            return {};
    }
};

template <class Type, value_type_t<Type> Value>
struct _ValidatedConstant {
    using type = Validated<Type, [] (const auto&, const auto& value) {
        return (value == Value);
    }>;
};

template <class Type, value_type_t<Type> Value>
using ValidatedConstant = typename _ValidatedConstant<Type, Value>::type;

}

#endif