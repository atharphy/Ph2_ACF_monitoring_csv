#ifndef BITSERIALIZATION__TYPES__SEPARATEDBY_HPP
#define BITSERIALIZATION__TYPES__SEPARATEDBY_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class Type, ignores_input_value SeparatorType>
struct SeparatedBy {
    static constexpr bool ignores_input_value = ignores_input_value<Type>;
    using value_type = ConvertibleVector<value_type_t<Type>>;

    static constexpr StringLiteral name = "SeparatedBy";

    using ParseError = ElementError<parse_error_t<Type>, name>;

    template <class T, class U=Void>
    static ParseResult<value_type, ParseError> 
    parse(const BitView<T>& bits, const U& parent={}) {
        value_type results;
        size_t offset = 0;
        while (true) {
            auto element = Type::parse(bits.slice(offset), parent);
            if (!element) 
                return ParseError{results.size(), std::move(element.error())};
            results.push_back(std::move(element.value()));
            offset += element.size();
            auto sep = SeparatorType::parse(bits.slice(offset));
            if (!sep) 
                break;
            offset += sep.size();
        }
        return {std::move(results), offset};
    }

    using SerializeElementError = ElementError<serialize_error_t<Type>, name>;

    using SerializeError = ErrorVariant<
        SerializeElementError, 
        serialize_error_t<SeparatorType>
    >;

    template <class T, class U=Void>
    static SerializeResult<SerializeError> 
    serialize(value_type& value, BitVector<T>& bits, const U& parent={}) {
        if (value.size() > 0) {
            for (size_t i = 0; i < value.size(); ++i) {
                auto element = Type::serialize(value[i], bits, parent);
                if (!element)
                    return {SerializeElementError{i, std::move(element.error())}};
                if (i < value.size() - 1) {
                    value_type_t<SeparatorType> v;
                    auto sep = SeparatorType::serialize(v, bits, parent);
                    if (!sep)
                        return {std::move(sep.error())};
                }
            }
        }
        return {};
    }
};

}

#endif