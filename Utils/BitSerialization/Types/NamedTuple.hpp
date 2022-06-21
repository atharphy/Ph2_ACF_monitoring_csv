#ifndef BITSERIALIZATION__TYPES__NAMEDTUPLE_HPP
#define BITSERIALIZATION__TYPES__NAMEDTUPLE_HPP

#include "../Core.hpp"
#include <type_traits>

#include <string.h>

#ifndef COMPILETIMESTRINGLITERAL
#define COMPILETIMESTRINGLITERAL

namespace compile_time_string_literal {

    // // CRC32 Table (zlib polynomial)
    // static constexpr uint32_t crc_table[256] = {
    //     0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
    //     0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
    //     0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
    //     0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    //     0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
    //     0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
    //     0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
    //     0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    //     0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
    //     0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
    //     0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
    //     0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    //     0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
    //     0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
    //     0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
    //     0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    //     0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
    //     0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
    //     0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
    //     0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    //     0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
    //     0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
    //     0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
    //     0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    //     0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
    //     0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
    //     0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
    //     0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    //     0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
    //     0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
    //     0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
    //     0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    //     0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
    //     0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
    //     0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
    //     0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    //     0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
    //     0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
    //     0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
    //     0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    //     0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
    //     0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
    //     0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
    //     0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    //     0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
    //     0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
    //     0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
    //     0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    //     0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
    //     0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
    //     0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
    //     0x2d02ef8dL
    // };
    // template<size_t idx>
    // constexpr uint32_t crc32(const char * str)
    // {
    //     return (crc32<idx-1>(str) >> 8) ^ crc_table[(crc32<idx-1>(str) ^ str[idx]) & 0x000000FF];
    // }

    // // This is the stop-recursion function
    // template<>
    // constexpr uint32_t crc32<size_t(-1)>(const char * str)
    // {
    //     return 0xFFFFFFFF;
    // }

    // // This doesn't take into account the nul char
    // #define COMPILE_TIME_CRC32_STR(x) (compile_time_string_literal::crc32<sizeof(x) - 2>(x) ^ 0xFFFFFFFF)


    // template <uint32_t Hash, size_t Size>
    // struct c_str {
    //     static constexpr uint32_t hash = Hash;
    //     static char value[Size];
    // };

    // template <uint32_t Hash, size_t Size>
    // char c_str<Hash, Size>::value[Size];

    // #pragma GCC diagnostic push
    // #pragma GCC diagnostic ignored "-Wpedantic"

    // template<typename Char, Char... Cs>
    // auto operator"" _s(){
    //     constexpr char str[] = {Cs..., 0};
    //     using result_type = c_str<COMPILE_TIME_CRC32_STR(str), sizeof(str)>;
    //     std::copy(std::begin(str), std::end(str), std::begin(result_type::value));
    //     return result_type{};
    // }

    // #pragma GCC diagnostic pop


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
    
    // static_assert(n_fields > 0);

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

        // template <class T = required_fields_tuple>
        //     requires(std::tuple_size_v<T> == 1)
        // operator std::tuple_element_t<0, values_tuple>&() {
        //     return std::get<0>(_values);
        // }
        
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
        
        // static constexpr std::array hashes = {COMPILE_TIME_CRC32_STR(Fields::key.value)...};
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



#include "../Core.hpp"


template <class... Fields>
std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const NamedTuple<Fields...>> obj) {
    return (os << obj.get());
}

}

#endif