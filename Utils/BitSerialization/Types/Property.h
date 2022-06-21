#ifndef BITSERIALIZATION__TYPES__PROPERTY_HPP
#define BITSERIALIZATION__TYPES__PROPERTY_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class ValueType, auto F>
struct Property {
    static constexpr bool ignores_input_value = true;

    template <class T, class U=Void>
    static ParseResult<ValueType> parse(const BitView<T>& bits, const U& parent={}) {
        return {ValueType(F(parent)), 0};
    }

    template <class T, class U=Void>
    static auto serialize(ValueType& value, BitVector<T>& bits, const U& parent={}) {
        value = F(parent);
        return SerializeResult{};
    }
};

}

#endif