#ifndef BITSERIALIZATION__TYPES__TRANSFORMED_HPP
#define BITSERIALIZATION__TYPES__TRANSFORMED_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class Type, auto Transform>
struct Transformed {
    static constexpr bool ignores_input_value = ignores_input_value_v<Type>;
    using value_type = decltype(
        Transform.decode(std::declval<value_type_t<Type>>())
    );


    template <class T, class U=Void>
    static ParseResult<value_type, parse_error_t<Type>> 
    parse(const BitView<T>& bits, const U& parent={}) {
        auto result = Type::parse(bits, parent);
        if (!result)
            return {std::move(result.error())};
        return {Transform.decode(result.value()), result.size()};
    }
    
    template <class T, class U=Void>
    static auto serialize(value_type& value, BitVector<T>& bits, const U& parent={}) {
        if constexpr (ignores_input_value_v<Type>) {
            value_type_t<Type> value_storage = {};
            auto result = Type::serialize(value_storage, bits, parent);
            if (result)
                value = Transform.decode(value_storage);
            return result;
        }
        auto encoded_value = Transform.encode(value);
        return Type::serialize(encoded_value, bits, parent);
    }
};

}

#endif