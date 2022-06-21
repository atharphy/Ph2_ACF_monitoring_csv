#ifndef BITSERIALIZATION__TYPES__ALIGNED_HPP
#define BITSERIALIZATION__TYPES__ALIGNED_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class Type, size_t Size>
struct Aligned {
    // static constexpr StringLiteral name = 
    //     concat_string_literals<"Aligned<", to_string_literal<Size>(), ">">();

    static constexpr bool ignores_input_value = ignores_input_value_v<Type>;

    using _SizeError = SizeError<"Aligned">;

    using ParseError = ErrorVariant<_SizeError, parse_error_t<Type>>;

    template <class T, class U=Void>
    static ParseResult<value_type_t<Type>, ParseError> 
    parse(const BitView<T>& bits, const U& parent={}) {
        auto result = Type::parse(bits, parent);
        if (!result)
            return {std::move(result.error())};
        size_t leftover_bits = result.size() % Size;
        if (leftover_bits > 0) {
            int padding = Size - leftover_bits;
            if (bits.size() < result.size() + padding)
                return {_SizeError(bits.size())};
            return {std::move(result.value()), result.size() + padding};
        }
        return {std::move(result.value()), result.size()};
    }

    struct SerializeSizeError {
        size_t size;

        friend std::ostream& operator<<(std::ostream& os, const SerializeSizeError& self) {
            return (os << "Padded error: too many bits (" << self.size << '/' << Size << ")");
        }
    };

    template <class T, class U=Void>
    static SerializeResult<serialize_error_t<Type>>
    serialize(value_type_t<Type>& value, BitVector<T>& bits, const U& parent={}) {
        size_t original_size = bits.size();
        auto result = Type::serialize(value, bits, parent);
        if (!result)
            return {std::move(result.error())};
        size_t leftover_bits = (bits.size() - original_size) % Size;
        if (leftover_bits > 0)
            bits.append_zeros(Size - leftover_bits);
        return {};
    }
};

}

#endif