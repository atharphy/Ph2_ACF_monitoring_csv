#ifndef BITSERIALIZATION_TYPES_NAMEDTUPLE_H
#define BITSERIALIZATION_TYPES_NAMEDTUPLE_H

#include "../Core.h"
#include <type_traits>
#include <string.h>

#ifndef COMPILETIMESTRINGLITERAL
#define COMPILETIMESTRINGLITERAL

namespace compile_time_string_literal {
    template <char... Chars>
    struct c_str {
        static const char value[sizeof...(Chars) + 1];
    };

    template <char... Chars>
    const char c_str<Chars...>::value[] = {Chars..., 0};

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"

    template<typename Char, Char... Cs>
    constexpr c_str<Cs...> operator"" _s(){
        return {};
    }

    #pragma GCC diagnostic pop
}

#endif

namespace BitSerialization {

template <StringLiteral Key, class T>
struct NamedField {
    using key_type = decltype(Key);
    static constexpr key_type key = Key;
    using type = T;
};

template <class T>
struct is_required : public std::integral_constant<bool, (!ignores_input_value_v<T>)> {};

template <class... Fields>
struct _NamedTuple {
    static constexpr size_t n_fields = sizeof...(Fields);
   
    using fields_tuple = std::tuple<Fields...>;
    using required_field_indices = indices_where_t<is_required, typename Fields::type...>;
    using required_fields_tuple = tuple_subset_t<fields_tuple, required_field_indices>;

    template <class>
    struct NamedTuple;

    template <class... RequiredFields>
    struct NamedTuple<std::tuple<RequiredFields...>> {
        static constexpr bool ignores_input_value = (ignores_input_value<typename Fields::type> && ...);

        static constexpr std::tuple<typename Fields::key_type...> keys = {Fields::key...};

        static constexpr std::tuple key_arrays = {Fields::key.value...};

        template <class Field>
        using value_type = value_type_t<typename Field::type, NamedTuple>;

        using values_tuple = std::tuple<value_type<Fields>...>;
        
        using required_values_tuple = tuple_subset_t<values_tuple, required_field_indices>;

        template <class Field, template <class, class> class error_getter>
        struct FieldError {
            error_getter<typename Field::type, NamedTuple>::type sub_error;

            friend std::ostream& operator<<(std::ostream& os, const FieldError& self) {
                os << "NamedTuple error (field \"" << &Field::key.value[0] << "\"): " << self.sub_error;
                return os;
            };
        };

        template <class Field>
        using FieldParseError = FieldError<Field, parse_error>;

        using ParseError = ErrorVariant<FieldParseError<Fields>...>;

        template <class T, class U=Void>
        static ParseResult<NamedTuple, ParseError> parse(const BitView<T>& bits, const U& parent={}) {
            return parse_impl(bits, std::make_index_sequence<n_fields>());
        }

    private:

        template <class T, size_t... Is>
        static ParseResult<NamedTuple, ParseError> 
        parse_impl(const BitView<T>& bits, std::index_sequence<Is...>) {
            NamedTuple object;
            ParseError error;
            size_t offset = 0;
            
            auto parse_field = 
                [&] <size_t I> (auto&& parse_result, std::integral_constant<size_t, I>) {
                    using Field = std::tuple_element_t<I, std::tuple<Fields...>>;
                    if (!parse_result) {
                        // std::cout << parse_result.error() << std::endl;
                        error = FieldParseError<Field>{std::move(parse_result.error())};
                        // std::cout << std::ref(object) << std::endl;
                        return false;
                    }
                    object.template get<I>() = std::move(parse_result.value());
                    offset += parse_result.size();
                    return true;
                };

            if ((
                parse_field(
                    std::tuple_element_t<Is, fields_tuple>::type::parse(bits.slice(offset), object),
                    std::integral_constant<size_t, Is>()
                ) && ...
            ))
                return {std::move(object), offset};
            else
                return {std::move(error)};
        }

        template <class Field>
        using FieldSerializeError = FieldError<Field, serialize_error>;

        using SerializeError = ErrorVariant<FieldSerializeError<Fields>...>;

    public:

        template <class T, class U=Void>
        static SerializeResult<SerializeError> 
        serialize(NamedTuple& value, BitVector<T>& bits, const U& parent={}) {
            return serialize_impl(value, bits, std::make_index_sequence<n_fields>());
        }

    private:

        template <class T, size_t... Is>
        static SerializeResult<SerializeError> 
        serialize_impl(NamedTuple& value, BitVector<T>& bits, std::index_sequence<Is...>) {
            SerializeError error;
            
            auto serialize_field = 
                [&] <size_t I> (auto&& serialize_result, std::integral_constant<size_t, I>) {
                    using Field = std::tuple_element_t<I, std::tuple<Fields...>>;
                    if (!serialize_result) {
                        error = FieldSerializeError<Field>{std::move(serialize_result.error())};
                        return false;
                    }
                    return true;
                };

            if ((
                serialize_field(
                    std::tuple_element_t<Is, fields_tuple>::type::serialize(
                        value.template get<Is>(), 
                        bits, 
                        value
                    ),
                    std::integral_constant<size_t, Is>()
                ) && ...
            ))
                return {};
            else
                return {std::move(error)};
        }

    public:
        NamedTuple() {}

        NamedTuple(value_type<Fields>... values) 
        : _values(std::move(values)...)
        {}
        
        template <size_t N = sizeof...(RequiredFields), typename std::enable_if_t<(N < n_fields), int> = 0>
        NamedTuple(value_type<RequiredFields>... args) {
            get_values_tuple(required_field_indices()) = std::forward_as_tuple(std::move(args)...);
        }

        template <class OtherNamedTuple>
            requires (
                std::is_same_v<decltype(OtherNamedTuple::key_arrays), decltype(key_arrays)>
                &&
                [] <size_t... Is> (std::index_sequence<Is...>) {
                    return (strings_equal(std::get<Is>(OtherNamedTuple::key_arrays), std::get<Is>(key_arrays)) && ...);
                } (std::index_sequence_for<Fields...>())
            )
        NamedTuple(const OtherNamedTuple other)
          : _values(other.values()) 
        {}
      
        template <class T = required_values_tuple>
            requires(std::tuple_size_v<T> == 1)
        operator std::tuple_element_t<0, required_values_tuple>() const {
            return [&] <size_t I, size_t... Is> (std::index_sequence<I, Is...>) {
                return std::get<I>(_values);
            } (required_field_indices());
        }

        template <size_t... Is>
        auto get_values_tuple(std::index_sequence<Is...>) {
            return std::tie(std::get<Is>(_values)...);
        }

        template <size_t... Is>
        const auto get_values_tuple(std::index_sequence<Is...>) const {
            return std::tie(std::get<Is>(_values)...);
        }

        friend bool operator==(const NamedTuple& a, const NamedTuple& b) {
            return a.get_values_tuple(required_field_indices()) == b.get_values_tuple(required_field_indices());
        }
        
        static constexpr std::array names = {Fields::key.value...};

        template <class Key>
        auto& operator[](Key) {
            constexpr auto i = std::find_if(std::begin(names), std::end(names), [] (auto str) {
                    return strings_equal(str, Key::value);
                }) - std::begin(names);
            return std::get<i>(_values);
        }

        template <class Key>
        const auto& operator[](Key) const {
            constexpr auto i = std::find_if(std::begin(names), std::end(names), [] (auto str) {
                    return strings_equal(str, Key::value);
                }) - std::begin(names);
            // constexpr auto i = std::find(std::begin(hashes), std::end(hashes), Key::hash) - std::begin(hashes);
            return std::get<i>(_values);
        }

        template <size_t I>
        auto& get() {
            return std::get<I>(_values);
        }

        template <size_t I>
        const auto& get() const {
            return std::get<I>(_values);
        }

        values_tuple& values() { return _values; }
        const values_tuple& values() const { return _values; }

        template <class T>
        auto serialize(BitVector<T>& bits) {
            return NamedTuple::serialize(*this, bits);
        }

        
        friend std::ostream& operator<<(std::ostream& os, const NamedTuple& obj) {
            os << "{" << increase_indent << endl_indent;
            [&] <size_t... Is> (std::index_sequence<0, Is...>) {
                os << '\"' << &std::get<0>(NamedTuple::keys).value[0] << "\": " << std::ref(obj.template get<0>());
                ((os << ',' << endl_indent << '\"' <<  &std::get<Is>(NamedTuple::keys).value[0] << "\": " << std::ref(obj.template get<Is>())), ...);
            }(std::index_sequence_for<Fields...>{});
            // f(std::make_index_sequence<Obj::n_fields>{});
            os << decrease_indent << endl_indent << "}";
            return os;
        }

    private:
        values_tuple _values;

    };

    using type = NamedTuple<required_fields_tuple>;
};

template <class... Fields>
struct get_object_type;

template <class Field, class... Fields>
struct get_object_type<Field, Fields...> {
    using type = _NamedTuple<Field, Fields...>::type;
};

template <>
struct get_object_type<> {
    using type = Void;
};

template <class... Fields>
using NamedTuple = get_object_type<Fields...>::type;

#include "../Core.h"

template <class... Fields>
std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const NamedTuple<Fields...>> obj) {
    return (os << obj.get());
}

}

#endif
