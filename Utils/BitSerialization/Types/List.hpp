#ifndef BITSERIALIZATION__TYPES__LIST_HPP
#define BITSERIALIZATION__TYPES__LIST_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class Type, auto SizeGetter>
struct List {
    static constexpr bool ignores_input_value = ignores_input_value_v<Type>;
    using value_type = ConvertibleVector<value_type_t<Type>>;

    // using ParseError = ElementError<parse_error_t<Type>, "List">;

    template <class T, class U=Void>
    static ParseResult<value_type, ElementError<parse_error_t<Type>, "List">>
    parse(const BitView<T>& bits, const U& parent={}) {
        size_t size = SizeGetter(parent);
        value_type results;
        results.reserve(size);
        size_t offset = 0;
        for (size_t i = 0; i < size; ++i) {
            auto parse_result = Type::parse(bits.slice(offset), parent);
            if (!parse_result) 
                return {{results.size(), std::move(parse_result.error())}};
            results.push_back(std::move(parse_result.value()));
            offset += parse_result.size();
        }
        return ParseResult<value_type, ElementError<parse_error_t<Type>, "List">>{std::move(results), offset};
    }

    using SerializeElementError = ElementError<serialize_error_t<Type>, "List">;

    struct SizeError {
        size_t required_size;
        size_t size;

        friend std::ostream& operator<<(std::ostream& os, const SizeError& error) {
            return (os << "List serialize error: not enough bits (" << error.size << " / " << error.required_size << ").");
        }
    };

    using SerializeError = ErrorVariant<SizeError, SerializeElementError>;

    template <class ValueType, class T, class U=Void>
    static SerializeResult<SerializeError>
    serialize(ValueType& value, BitVector<T>& bits, const U& parent={}) {
        size_t size = SizeGetter(parent);
        if (size != value.size())
            return {SizeError{size, value.size()}};
        for (size_t i = 0; i < value.size(); ++i) {
            auto result = Type::serialize(value[i], bits, parent);
            if (!result) 
                return {SerializeElementError{i, std::move(result.error())}};
        }
        return {};
    }
};

}

#endif