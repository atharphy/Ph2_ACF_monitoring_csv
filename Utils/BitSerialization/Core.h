#ifndef BITSERIALIZATION_CORE_H
#define BITSERIALIZATION_CORE_H

#include "BitVector.h"
#include "Utility.h"
#include "Results.h"
#include "Printing.h"

namespace BitSerialization {

struct Void;

template <class Type, class Parent=Void>
using parse_result_t = decltype(Type::parse(std::declval<BitView<uint32_t>>(), std::declval<const Parent&>()));

template <class Type, class Parent=Void>
using value_type_t = typename parse_result_t<Type, Parent>::value_type;

template <class Type, class Parent=Void>
using parse_error_t = typename parse_result_t<Type, Parent>::error_type;

template <class Type, class Parent=Void>
struct parse_error { using type = parse_error_t<Type, Parent>; };

template <class Type, class Parent=Void>
using serialize_result_t = decltype(Type::serialize(
    std::declval<value_type_t<Type>&>(), 
    std::declval<BitVector<uint32_t>&>(), 
    std::declval<const Parent&>()
));

template <class Type, class Parent=Void>
using serialize_error_t = typename serialize_result_t<Type, Parent>::error_type;

template <class Type, class Parent=Void>
struct serialize_error { using type = serialize_error_t<Type, Parent>; };

template <typename T>
concept ignores_input_value = std::same_as<
    std::integral_constant<bool, T::ignores_input_value>, 
    std::integral_constant<bool, 1>
>;

template <typename T>
constexpr bool ignores_input_value_v = ignores_input_value<T>;




struct Void {
    static constexpr bool ignores_input_value = true;

    template <class T, class U=Void>
    static ParseResult<Void> parse(const BitView<T>& bits, const U& parent={}) {
        return {Void(), 0};
    }

    template <class T, class U=Void>
    static SerializeResult<> 
    serialize(Void& value, BitVector<T>& bits, const U& parent={}) {
        return {};
    }
};

inline std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const Void> wrapper) {
    return (os << "Void");
}

struct EndOfStream {
    static constexpr bool ignores_input_value = true;

    struct ParseError {
        friend std::ostream& operator<<(std::ostream& os, const ParseError& wrapper) {
            return (os << "EndOfStream error.");
        }
    };

    template <class T, class U=Void>
    static ParseResult<Void, ParseError> parse(const BitView<T>& bits, const U& parent={}) {
        if (bits.size() > 0)
            return ParseError{};
        return {Void(), 0};
    }

    template <class T, class U=Void>
    static SerializeResult<> 
    serialize(Void& value, BitVector<T>& bits, const U& parent={}) {
        return {};
    }
};


template <size_t N>
struct Uint {
    static constexpr StringLiteral name = 
        concat_string_literals<"Uint<", to_string_literal<N>(), ">">();

    using value_type = uint_t<N>;

    using ParseError = SizeError<name>;

    template <class T, class U=Void>
    static ParseResult<value_type, ParseError> 
    parse(const BitView<T>& bits, const U& parent={}) {
        if (bits.size() < N)
            return ParseError{bits.size()};
        else
            return {bits.slice(0, N).template get<value_type>(), N};
    }

    template <class T, class U=Void>
    static SerializeResult<> 
    serialize(const value_type& value, BitVector<T>& bits, const U& parent={}) {
        bits.append_zeros(N).template set<value_type>(value);
        return {};
    }
};

using Bool = Uint<1>;

template <class BlockType, auto SizeGetter>
struct RawBits {
    static constexpr StringLiteral name = "RawBits";
        // concat_string_literals<"RawBits">();

    using value_type = BitView<BlockType>;

    using ParseError = SizeError<name>;

    template <class T, class U=Void>
    static ParseResult<value_type, ParseError> 
    parse(const BitView<T>& bits, const U& parent={}) {
        size_t size = SizeGetter(parent);
        if (bits.size() < size)
            return ParseError{bits.size()};
        else
            return {bits.slice(0, size), size};
    }

    template <class T, class U=Void>
    static SerializeResult<> 
    serialize(const value_type& value, BitVector<T>& bits, const U& parent={}) {
        bits.append(value);
        return {};
    }
};

template <auto SizeGetter>
struct DontCare {
    static constexpr StringLiteral name = "DontCare";
    
    using ParseError = SizeError<name>;

    template <class T, class U=Void>
    static ParseResult<Void, ParseError>  
    parse(const BitView<T>& bits, const U& parent={}) {
        size_t size = SizeGetter(parent);
        if (bits.size() < size)
            return ParseError{bits.size()};
        else
            return {Void(), size};
    }

    template <class T, class U=Void>
    static SerializeResult<> 
    serialize(Void& value, BitVector<T>& bits, const U& parent={}) {
        bits.append_zeros(SizeGetter(parent));
        return {};
    }
};

// template <class Type, class T, class U=Void>
// auto serialize(value_type_t<Type>& value, BitVector<T>& bits, const U& parent={}) {
//     Type::serialize(value, bints, parent)
// }

// template <class Type, class Value, class T, class U=Void>
//     requires(!std::same_as<Value, value_type_t<Type>>) 
// auto serialize(Value& value, BitVector<T>& bits, const U& parent={}) {
//     value_type_t<Type> converted_value = value;
//     auto result = Type::serialize(converted_value, bints, parent);
//     value = converted_value;
//     return result;
// }


}

#endif
