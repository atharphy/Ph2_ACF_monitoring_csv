#ifndef BITSERIALIZATION__TYPES__CLASSOBJECT_HPP
#define BITSERIALIZATION__TYPES__CLASSOBJECT_HPP

#include "../Core.hpp"
#include <type_traits>


namespace BitSerialization {
    
namespace detail {

    template<class P> struct member_ptr_info;

    template<class C, class T>
    struct member_ptr_info<T C::*>{
        using cls_type = C;
        using value_type = T;
    };

    template <class Field>
    struct is_member {
        static constexpr bool value = std::is_member_pointer_v<typename Field::key_type>;
    };

    template <class Field>
    constexpr bool is_member_v = is_member<Field>::value;

    template <class... Fields>
    auto get_common_type(std::tuple<Fields...>) -> std::common_type_t<typename member_ptr_info<typename Fields::key_type>::cls_type...>;

    template < class... Fields>
    struct member_fields_common_type {
        using type = decltype(get_common_type(std::declval<filtered_tuple_t<is_member, Fields...>>()));
    };

    template <class... Fields>
    using member_fields_common_type_t = typename member_fields_common_type<Fields...>::type;

}

struct Discarded {};


template <auto Key, class Type>
struct Field {
    static constexpr auto key = Key;
    using key_type = decltype(Key);
    using type = Type;
};

template <class... Fields>
struct ClassObject : public detail::member_fields_common_type_t<Fields...> {
    using value_type = detail::member_fields_common_type_t<Fields...>;

    
    using members_tuple = filtered_tuple_t<detail::is_member, Fields...>;
    static const size_t size = std::tuple_size_v<members_tuple>;

    ClassObject() : value_type() {}
    ClassObject(const value_type& value) : value_type(value) {}
    ClassObject(value_type&& value) : value_type(std::move(value)) {}
    ClassObject(const ClassObject& other) : value_type(other) {}
    ClassObject(ClassObject&& other) : value_type(std::move(other)) {}
    ClassObject& operator=(const value_type& value) { value_type::operator=(value); return *this; }
    ClassObject& operator=(value_type&& value) { value_type::operator=(std::move(value)); return *this; }
    ClassObject& operator=(const ClassObject& other) { value_type::operator=(other); return *this; }
    ClassObject& operator=(ClassObject&& other) { value_type::operator=(std::move(other)); return *this; }
    
    template <class Field>
    using FieldParseError = ElementError<parse_error_t<typename Field::type>, "ClassObject">;

    using ParseError = ErrorVariant<FieldParseError<Fields>...>;

    template <class T, class U=Void>
    static ParseResult<ClassObject, ParseError> parse(const BitView<T>& bits, const U& parent={}) {
        ClassObject object;
        ParseError error;
        size_t offset = 0;
        size_t field_index = 0;
        
        auto parse_field = [&] (auto field, size_t i) {
            using Field = decltype(field);
            auto result = Field::type::parse(bits.slice(offset), object);
            if (!result) {
                error = FieldParseError<Field>{i, std::move(result.error())};
                return false;
            }
            if constexpr (detail::is_member_v<Field>)
                object.*(Field::key) = std::move(result.value());
            offset += result.size();
            return true;
        };

        if ((parse_field(Fields{}, field_index++) && ...))
            return {std::move(object), offset};
        else
            return {std::move(error)};
    }

    template <class Field>
    using FieldSerializeError = ElementError<serialize_error_t<typename Field::type>, "ClassObject">;

    using SerializeError = ErrorVariant<FieldSerializeError<Fields>...>;

    template <class T, class U=Void>
    static SerializeResult<SerializeError> 
    serialize(value_type& value, BitVector<T>& bits, const U& parent={}) {
        SerializeError error;
        size_t field_index = 0;
        
        auto serialize_field = [&] (auto field, size_t i) {
            using Field = decltype(field);
            using Encoding = typename Field::type;
            auto serialize_field_value = [&] (value_type_t<Encoding>&& field_value) {
                auto result = Encoding::serialize(field_value, bits, value);
                if (!result) {
                    error = FieldSerializeError<Field>{i, std::move(result.error())};
                    return false;
                }
                return true;
            };
            if constexpr (detail::is_member_v<Field>)
                return serialize_field_value(std::move(value.*(Field::key)));
            else
                return serialize_field_value({});
        };

        if ((serialize_field(Fields{}, field_index++) && ...))
            return {};
        else
            return {std::move(error)};
    }

    template <class T>
    auto serialize(BitVector<T>& bits) {
        ClassObject::serialize(*this, bits);
    }


    friend std::ostream& operator<<(std::ostream& os, const ClassObject& value)
    {
        os << "{" << increase_indent << endl_indent;
        [&] <size_t... Is> (std::index_sequence<Is...>) {
            ((os << std::ref(value.*(std::tuple_element_t<Is, members_tuple>::key)), (Is == size - 1 ? os << "" : os << ',' << endl_indent)), ...);
        }(std::make_index_sequence<size>{});
        os << decrease_indent << endl_indent << "}";
        return os;
    }

    friend std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const ClassObject> value)
    {
        return os << value.get();
    }
};

// template <class... Fields>
// std::ostream& operator<< (std::ostream& os, std::reference_wrapper<const typename ClassObject<Fields...>::value_type> value) {
//     os << "(" << increase_indent << endl_indent;
//     [&] <size_t... Is> (std::index_sequence<0, Is...>) {
//         ((os << value.get().*(Fields::ptr) << (Is == sizeof...(Fields) - 1 ? "" : "\n")), ...);
//     }(std::index_sequence_for<Fields...>{});
//     os << decrease_indent << endl_indent << "}";
//     return os;
// }

// template <class X>
// std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const value_type>& value) {

// }


}

#endif