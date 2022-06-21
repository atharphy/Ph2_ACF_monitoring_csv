#ifndef BITSERIALIZATION__TYPES__ONEOF_HPP
#define BITSERIALIZATION__TYPES__ONEOF_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class... Types>
struct OneOf {
    static constexpr bool ignores_input_value = (ignores_input_value<Types> && ...);
    using value_type = std::common_type_t<typename Types::value_type...>;

    template <class... SubErrors>
    struct Error {
        std::tuple<SubErrors...> errors;

        friend std::ostream& operator<<(std::ostream& os, const Error& self) {
            os << "OneOf error (all types failed): ";
            [&] <size_t... Is> (std::index_sequence<Is...>) {
                ((os << std::get<Is>(self.errors) << "; "), ...);
            }(std::index_sequence_for<SubErrors...>{});
                // [&] <size_t I> (std::integral_constant<size_t, I>) { os << std::get<I>(self.errors) << "; "; }(Is)
            // std::apply(
            //     [&] (auto... errors) {
            //         ((os << errors << "; "), ...);
            //     },
            //     self.errors
            // );
            return os;
        }
    };

    using ParseError = Error<parse_error_t<Types>...>;

    template <class T, class U, size_t... Is>
    static ParseResult<value_type, ParseError> 
    parse_impl(const BitView<T>& bits, const U& parent, std::index_sequence<Is...>) {
        ParseError error;        
        value_type overall_result;
        size_t size = 0;

        auto process_one = [&] <size_t I> (
            auto&& result, 
            std::integral_constant<size_t, I>
        ) {
            if (result) {
                overall_result = std::move(result.value());
                size = result.size();
                return true;
            }
            else {
                std::get<I>(error.errors) = std::move(result.error());
                return false;
            }
        };

        if ((process_one(
            Types::parse(bits, parent), 
            std::integral_constant<size_t, Is>()
            ) || ...))
            return {std::move(overall_result), size};
        else
            return {std::move(error)};
    }

    
    template <class T, class U=Void>
    static auto parse(const BitView<T>& bits, const U& parent={}) {
        return parse_impl(bits, parent, std::make_index_sequence<sizeof...(Types)>());
    }
    
    using SerializeError = Error<serialize_error_t<Types>...>;

    template <class T, class U, size_t... Is>
    static SerializeResult<SerializeError> 
    serialize_impl(
        value_type& value, 
        BitVector<T>& bits, 
        const U& parent, 
        std::index_sequence<Is...>
    ) {
        SerializeError error;  

        auto process_one = [&] <size_t I> (auto&& result, std::integral_constant<size_t, I>) {
            if (result)
                return true;
            else {
                std::get<I>(error.errors) = std::move(result.error());
                return false;
            }
        };

        if ((process_one(
                Types::serialize(value, bits, parent), 
                std::integral_constant<size_t, Is>()
            ) || ...))
            return {};
        else
            return {std::move(error)};
    }

    template <class T, class U=Void>
    static auto serialize(value_type& value, BitVector<T>& bits, const U& parent={}) {
        return serialize_impl(value, bits, parent, std::make_index_sequence<sizeof...(Types)>());
    }
};

}

#endif