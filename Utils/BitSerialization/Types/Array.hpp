#ifndef BITSERIALIZATION__TYPES__ARRAY_HPP
#define BITSERIALIZATION__TYPES__ARRAY_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class Type, size_t Size>
struct Array {
    static constexpr bool ignores_input_value = ignores_input_value_v<Type>;
    using value_type = std::array<value_type_t<Type>, Size>;

    using ParseError = ElementError<parse_error_t<Type>, "Array">;

    template <class T, class U=Void>
    static ParseResult<value_type, ParseError> 
    parse(const BitView<T>& bits, const U& parent={}) {
        ParseResult<value_type, ParseError>  result;
        size_t offset = 0;
        for (size_t i = 0; i < Size; ++i) {
            auto parse_result = Type::parse(bits.slice(offset), parent);
            if (!parse_result) {
                return ParseError{i, std::move(parse_result.error())};
            }
            result.value()[i] = std::move(parse_result.value());
            offset += parse_result.size();
        }
        result.size() = offset;
        return result;
    }

    using SerializeError = ElementError<serialize_error_t<Type>, "Array">;

    template <class ValueType, class T, class U=Void>
    static SerializeResult<SerializeError>
    serialize(ValueType& value, BitVector<T>& bits, const U& parent={}) {
        for (size_t i = 0; i < value.size(); ++i) {
            auto result = Type::serialize(value[i], bits, parent);
            if (!result) 
                return SerializeError{i, std::move(result.error())};
        }
        return {};
    }
};

}

#endif