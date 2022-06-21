#ifndef BITSERIALIZATION__TYPES__CONSTANT_HPP
#define BITSERIALIZATION__TYPES__CONSTANT_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class Type, value_type_t<Type> Value>
struct Constant {
    static constexpr bool ignores_input_value = true;

    using value_type = value_type_t<Type>;

    using _ValueError = ValueError<"Constant", value_type>;

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
        if (result.value() != Value)
            return {_ValueError{std::move(result.value())}};
        return {std::move(result.value()), result.size()};
    }

    template <class T, class U=Void>
    static auto serialize(const value_type&, BitVector<T>& bits, const U& parent={})
    {
        value_type value = Value;
        return Type::serialize(value, bits, parent);
    }
};

}

#endif