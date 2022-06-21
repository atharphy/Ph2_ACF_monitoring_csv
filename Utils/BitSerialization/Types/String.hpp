#ifndef BITSERIALIZATION__TYPES__STRING_HPP
#define BITSERIALIZATION__TYPES__STRING_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class ElementType, class TerminatorType, bool IncludeTerminator=true>
struct String {
    static constexpr bool ignores_input_value = ignores_input_value_v<ElementType>;

    // template <bool B, class... Ts>
    // struct get_type;

    // template <class... Ts>
    // struct get_type<true, Ts...> {
    //     using type = std::common_type_t<value_type_t<Ts>...>;
    // };

    // template <class T, class... Ts>
    // struct get_type<false, T, Ts...> {
    //     using type = value_type_t<T>;
    // };

    // using element_value_type = typename get_type<IncludeTerminator, ElementType, TerminatorType>::type;
    // std::conditional_t<
    //     IncludeTerminator,
    //     std::common_type_t<
    //         value_type_t<ElementType>, 
    //         value_type_t<TerminatorType>
    //     >,
    //     value_type_t<ElementType>
    // >;

    using value_type = ConvertibleVector<value_type_t<ElementType>>;

    using ParseError = ElementError<parse_error_t<ElementType>, "String">;

    template <class T, class U=Void>
    static ParseResult<value_type, ParseError> 
    parse(const BitView<T>& bits, const U& parent={}) {
        value_type results;
        size_t offset = 0;
        while (true) {
            auto terminator_result = TerminatorType::parse(bits.slice(offset), parent);
            if (terminator_result) {
                if constexpr (IncludeTerminator) {
                    results.push_back(
                        value_type_t<ElementType>(std::move(terminator_result.value()))
                    );
                    offset += terminator_result.size();
                }
                break;
            }
            auto element_result = ElementType::parse(bits.slice(offset), parent);
            if (!element_result) {
                return ParseError{results.size(), std::move(element_result.error())};
            }
            results.push_back(std::move(element_result.value()));
            offset += element_result.size();
        }
        return {std::move(results), offset};
    }

    using SerializeElementError = ElementError<serialize_error_t<ElementType>, "String">;

    using SerializeError = ErrorVariant<
        SerializeElementError,
        serialize_error_t<TerminatorType>
    >;

    template <class T, class U=Void>
    static SerializeResult<SerializeError>
    serialize(value_type& value, BitVector<T>& bits, const U& parent={}) {
        for (size_t i = 0; i < value.size() - IncludeTerminator; ++i) {
            auto result = ElementType::serialize(value[i], bits, parent);
            if (!result)
                return {SerializeElementError{i, std::move(result.error())}};
        }
        if constexpr (IncludeTerminator) {
            auto v = TerminatorType(value.back());
            auto result = TerminatorType::serialize(v, bits, parent);
            if (!result)
                return {std::move(result.error())};
        }
        return {};
    }
};

}

#endif
