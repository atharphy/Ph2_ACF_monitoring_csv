#ifndef BITSERIALIZATION__TYPES__COMPOSE3_HPP
#define BITSERIALIZATION__TYPES__COMPOSE3_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class SourceType, class IntermediateType, class DestinationType>
struct Compose3 {
    static constexpr bool ignores_input_value = ignores_input_value<SourceType>;
    using value_type = value_type_t<SourceType>;
    
    template <size_t I, class SubError>
    struct Error {
        static constexpr std::array stage_names = {"source", "intermediate", "destination"};
        
        SubError error;

        Error(SubError&& error) : error(std::move(error)) {}

        friend std::ostream& operator<<(std::ostream& os, const Error& self) {
            return (os << "Compose3 " << stage_names[I] << " error: " << self.error);
        }
    };

    template <size_t I, class SubError>
    static Error<I, SubError> make_error(SubError&& error) {
        return {std::move(error)};
    }

    using ParseError = ErrorVariant<
        Error<0, parse_error_t<SourceType>>,
        Error<1, serialize_error_t<IntermediateType>>,
        Error<2, parse_error_t<DestinationType>>
    >;

    template <class T, class U=Void>
    static ParseResult<value_type, ParseError> 
    parse(const BitView<T>& bits, const U& parent={}) {
        BitVector<uint64_t> new_bits;
        
        // std::cout << "Compose3 input: " << bits << std::endl;

        auto destination_result = DestinationType::parse(bits, parent);
        if (!destination_result)
            return {make_error<2>(std::move(destination_result.error()))};
        // std::cout << "Compose3 value: " << std::ref(destination_result.value()) << std::endl;
        
        value_type_t<IntermediateType>&& value = value_type_t<IntermediateType>(std::move(destination_result.value()));
        
        // std::cout << "Compose3 parse value: " << std::ref(value) << std::endl;

        auto intermediate_result = IntermediateType::serialize(value, new_bits, parent);

        if (!intermediate_result)
            return {make_error<1>(std::move(intermediate_result.error()))};

            
        // std::cout << "Compose3 parse buffer: " << new_bits.view() << std::endl;

        auto source_result = SourceType::parse(bit_view(new_bits), parent);
        if (!source_result)
            return {make_error<0>(std::move(source_result.error()))};

        return {std::move(source_result.value()), destination_result.size()};
    }

    using SerializeError = ErrorVariant<
        Error<0, serialize_error_t<SourceType>>,
        Error<1, parse_error_t<IntermediateType>>,
        Error<2, serialize_error_t<DestinationType>>
    >;
    
    template <class T, class U=Void>
    static SerializeResult<SerializeError> 
    serialize(value_type& value, BitVector<T>& bits, const U& parent={}) {
        BitVector<uint64_t> new_bits;
        
        // std::cout << "Compose3 input: " << std::ref(value) << std::endl;

        auto source_result = SourceType::serialize(value, new_bits, parent);
        if (!source_result)
            return {make_error<0>(std::move(source_result.error()))};
        
        // std::cout << "Compose3 serialize buffer: " << new_bits.view() << std::endl;

        auto intermediate_result = IntermediateType::parse(bit_view(new_bits), parent);
        if (!intermediate_result)
            return {make_error<1>(std::move(intermediate_result.error()))};
            
        // std::cout << "Compose3 serialize intermediate value: " << std::ref(intermediate_result.value()) << std::endl;
        value_type_t<DestinationType>&& new_value = std::move(intermediate_result.value());

        auto destination_result = DestinationType::serialize(new_value, bits, parent);
        if (!destination_result)
            return {make_error<2>(std::move(destination_result.error()))};
        
        return {};
    }
};

}

#endif