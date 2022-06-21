#ifndef BITSERIALIZATION_TYPES_DYNAMIC_H
#define BITSERIALIZATION_TYPES_DYNAMIC_H

#include "../Core.h"

namespace BitSerialization {
    
template <class Type, auto ValueGetter>
struct Dynamic {
    static constexpr bool ignores_input_value = true;

    using value_type = value_type_t<Type>;

    using _ValueError = ValueError<"Dynamic", value_type>;

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
        if constexpr (!std::is_same_v<U, Void>)
            if (result.value() != ValueGetter(parent))
                return {_ValueError{std::move(result.value())}};
        return {std::move(result.value()), result.size()};
    }
    
    template <class T, class U=Void>
    static auto serialize(value_type& value, BitVector<T>& bits, const U& parent={})
    {
        if constexpr (!std::is_same_v<U, Void>)
            value = ValueGetter(parent);
        return Type::serialize(value, bits, parent);
    }
};

}

#endif
