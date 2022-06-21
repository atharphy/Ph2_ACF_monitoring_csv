#ifndef BITSERIALIZATION__TYPES__OPTIONAL_HPP
#define BITSERIALIZATION__TYPES__OPTIONAL_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class Type, auto Condition>
struct Optional {
    static constexpr bool ignores_input_value = ignores_input_value<Type>;

    using value_type = std::optional<value_type_t<Type>>;

    template <class T, class U>
    static ParseResult<value_type, parse_error_t<Type>> 
    parse(const BitView<T>& bits, const U& parent) {
        if (Condition(parent)) {
            auto result = Type::parse(bits, parent);
            if (result)
                return {std::move(result.value()), result.size()};
            else
                return {std::move(result.error())};
        }
        else {
            return {std::nullopt, 0};
        }
    }

    using SerializeError = serialize_error_t<Type>;

    template <class T, class U=Void>
    static SerializeResult<serialize_error_t<Type>> 
    serialize(value_type& value, BitVector<T>& bits, const U& parent={}) {
        if (Condition(parent)) {
            auto v = value.value();
            auto result = Type::serialize(v, bits, parent);
            if (!result)
                return {std::move(result.error())};
            else {
                value = v;
                return {};
            }
        }
        else {
            return {};
        }
    }
};

}

#endif