#ifndef BITSERIALIZATION__TYPES__Unserialized_HPP
#define BITSERIALIZATION__TYPES__Unserialized_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class Type>
struct Unserialized {
    static constexpr bool ignores_input_value = true;

    using value_type = value_type_t<Type>;

    using ParseError = parse_error_t<Type>;

    template <class T, class U=Void>
    static auto parse(const BitView<T>& bits, const U& parent={})
    {
        return Type::parse(bits, parent);
    }

    template <class T, class U=Void>
    static auto serialize(const value_type&, BitVector<T>& bits, const U& parent={})
    {
        return SerializeResult{};
    }
};

}

#endif