#ifndef BITSERIALIZATION__TYPES__STREAM_HPP
#define BITSERIALIZATION__TYPES__STREAM_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class InitiatorType, class ElementType, bool IncludeInitiator=true>
struct Stream {
    static constexpr bool ignores_input_value = ignores_input_value_v<ElementType>;

    using value_type = ConvertibleVector<value_type_t<ElementType>>;

    using InitiatorParseError = ElementError<parse_error_t<InitiatorType>, "Stream">;

    using ParseError = ErrorVariant<
        InitiatorParseError,
        ElementError<parse_error_t<ElementType>, "Stream">
    >;

    template <class T, class U=Void>
    static ParseResult<value_type, ParseError> 
    parse(const BitView<T>& bits, const U& parent={}) {
        value_type results;
        size_t offset = 0;
        auto initiator_result = InitiatorType::parse(bits.slice(offset), parent);
        if (!initiator_result)
            return {InitiatorParseError{IncludeInitiator - 1, std::move(initiator_result.error())}};
        offset += initiator_result.size();
        if (IncludeInitiator)
            results.push_back(initiator_result.value());
        while (offset < bits.size()) {
            auto element_result = ElementType::parse(bits.slice(offset), parent);
            if (!element_result)
                break;
            results.push_back(std::move(element_result.value()));
            offset += element_result.size();
        }
        return {std::move(results), offset};
    }

    using InitiatorSerializeError = ElementError<serialize_error_t<InitiatorType>, "Stream">;
    using ElementSerializeError = ElementError<serialize_error_t<ElementType>, "Stream">;

    using SerializeError = ErrorVariant<InitiatorSerializeError, ElementSerializeError>;

    template <class T, class U=Void>
    static SerializeResult<SerializeError>
    serialize(value_type& value, BitVector<T>& bits, const U& parent={}) {
        auto initiator_value = IncludeInitiator ? value_type_t<InitiatorType>(value[0]) : value_type_t<InitiatorType>{};
        auto initiator_result = InitiatorType::serialize(initiator_value, bits, parent);
        if (!initiator_result)
            return {InitiatorSerializeError{IncludeInitiator - 1, std::move(initiator_result.error())}};
        for (size_t i = IncludeInitiator ? 1 : 0; i < value.size(); ++i) {
            auto element_result = ElementType::serialize(value[i], bits, parent);
            if (!element_result)
                return {ElementSerializeError{i, std::move(element_result.error())}};
        }
        return {};
    }
};

}

#endif
