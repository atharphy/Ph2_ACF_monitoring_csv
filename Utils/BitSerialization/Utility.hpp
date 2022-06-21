#ifndef BITSERIALIZATION__UTILITIY_HPP
#define BITSERIALIZATION__UTILITIY_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <algorithm>

// #include "compile_time_string_literal.hpp"

namespace BitSerialization {
    
// using namespace compile_time_string_literal;

template <class T>
struct ConvertibleVector : public std::vector<T> {
    ConvertibleVector() {}

    ConvertibleVector(size_t count, const T& value) : std::vector<T>(count, value) {}

    explicit ConvertibleVector(size_t count) : std::vector<T>(count) {}

    template< class InputIt >
    ConvertibleVector(InputIt first, InputIt last) : std::vector<T>(first, last) {}

    ConvertibleVector(const ConvertibleVector& other) : std::vector<T>(other) {}

    ConvertibleVector(ConvertibleVector&& other) : std::vector<T>(std::move(other)) {}

    ConvertibleVector(std::initializer_list<T> init) : std::vector<T>(std::move(init)) {}

    template <class U, typename std::enable_if_t<std::is_convertible<U, T>::value, int> = 0>
    ConvertibleVector(const std::vector<U>& vec) : std::vector<T>(vec.begin(), vec.end()) {}
    
    template <class U> //, typename std::enable_if_t<std::is_convertible<U, T>::value, int> = 0>
    ConvertibleVector(const ConvertibleVector<U>& vec) {
        T first{vec[0]};
        this->push_back(first);
    }

    ConvertibleVector(std::vector<T>&& vec) : std::vector<T>(std::move(vec)) {}

    auto& operator=(const ConvertibleVector& other) {
        std::vector<T>::operator=(other);
        return *this;
    }

    auto& operator=(ConvertibleVector&& other) {
        std::vector<T>::operator=(std::move(other));
        return *this;
    }

    template <class U, typename std::enable_if_t<std::is_convertible<T, U>::value, int> = 0>
    operator ConvertibleVector<U>() {
        return {this->begin(), this->end()};
    }

    template <class U, typename std::enable_if_t<std::is_convertible<T, U>::value, int> = 0>
    operator std::vector<U>() {
        return {this->begin(), this->end()};
    }
};

// using uint128_t = unsigned __int128;

template <size_t N>
using uint_t =  std::conditional_t<N <=  8, uint8_t,
                std::conditional_t<N <= 16, uint16_t,
                std::conditional_t<N <= 32, uint32_t, uint64_t>>>;


template<size_t N>
struct StringLiteral {
    static constexpr size_t size = N;

    constexpr StringLiteral() {}
    
    // template <class T>
    // constexpr StringLiteral(const T& other)
    // //  : value(other.value)
    // {
    //     std::copy_n(other.value, N, value);
    // }

    constexpr StringLiteral(const char (&str)[N]) : value() {
        std::copy_n(str, N, value);
    }
    
    char value[N];
};

template<class T>
StringLiteral(const T&) -> StringLiteral<T::size>;

// template <class T>
// constexpr StringLiteral<T::size> copy(const T&) {
//     return T;
// }


constexpr bool strings_equal(char const * a, char const * b) {
    return *a == *b && (*a == '\0' || strings_equal(a + 1, b + 1));
}

template <size_t N, size_t base=10>
constexpr auto to_string_literal() {
    constexpr char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buf[([] {
              unsigned int len = N >= 0 ? 1 : 2; // Need one byte for '\0', two if there'll be a minus
              for (auto n = N < 0 ? -N : N; n; len++, n /= base);
              return len;
          }())];
    auto ptr = buf + sizeof(buf) / sizeof(buf[0]);
    *--ptr = '\0';
    for (auto n = N < 0 ? -N : N; n; n /= base)
        *--ptr = digits[n % base];
    if (N < 0)
        *--ptr = '-';
    return StringLiteral(buf);
}


template <StringLiteral... Strings>
constexpr auto concat_string_literals() {
    StringLiteral<((Strings.size - 1) + ...) + 1> result({});
    auto it = std::begin(result.value);
    ((it = std::copy(std::begin(Strings.value), std::end(Strings.value) - 1, it)), ...);
    return result;
}

template <typename... >
struct index_of;

// found it
template <typename T, typename... R>
struct index_of<T, T, R...>
: std::integral_constant<size_t, 0>
{ };

// still looking
template <typename T, typename F, typename... R>
struct index_of<T, F, R...>
: std::integral_constant<size_t, 1 + index_of<T,R...>::value>
{ };

template <class T, class... Ts>
std::tuple<T, Ts...> _tuple_prepend(std::tuple<Ts...>);

template <class T, class Tuple>
using tuple_prepend_t = decltype(_tuple_prepend<T>(Tuple()));

template <template <class> class Predicate, class... Ts>
struct filtered_tuple;

template <template <class> class Predicate, class... Ts>
using filtered_tuple_t = filtered_tuple<Predicate, Ts...>::type;

template <template <class> class Predicate, class T, class... Ts>
struct filtered_tuple<Predicate, T, Ts...> {
    using type = std::conditional_t<
        Predicate<T>::value,
        tuple_prepend_t<T, filtered_tuple_t<Predicate, Ts...>>,
        filtered_tuple_t<Predicate, Ts...>
    >;
};

template <template <class> class Predicate>
struct filtered_tuple<Predicate> {
    using type = std::tuple<>;
};


template <size_t I, size_t... Is>
std::index_sequence<I, Is...> _index_seq_prepend(std::index_sequence<Is...>);

template <size_t I, class Seq>
using index_seq_prepend_t = decltype(_index_seq_prepend<I>(Seq()));

template <template <class> class Predicate, size_t I, class... Ts>
struct indices_where;

template <template <class> class Predicate, class... Ts>
using indices_where_t = indices_where<Predicate, 0, Ts...>::type;

template <template <class> class Predicate, size_t I, class T, class... Ts>
struct indices_where<Predicate, I, T, Ts...> {
    using type = std::conditional_t<
        Predicate<T>::value,
        index_seq_prepend_t<I, typename indices_where<Predicate, I + 1, Ts...>::type>,
        typename indices_where<Predicate, I + 1, Ts...>::type
    >;
};

template <template <class> class Predicate, size_t I>
struct indices_where<Predicate, I> {
    using type = std::index_sequence<>;
};

template <class Tuple, size_t... Is>
std::tuple<std::tuple_element_t<Is, Tuple>...> _tuple_subset(std::index_sequence<Is...>);

template <class Tuple, class Seq>
using tuple_subset_t = decltype(_tuple_subset<Tuple>(Seq()));


// template <class Tuple, class... Ts>
// struct 

constexpr unsigned cilog2(unsigned val) { return val ? 1 + cilog2(val >> 1) : -1; }

// template <class Encode, class Decode>
// struct Transform {
//     Transform(Encode encode, Decode decode) 
//       : _encode(encode)
//       , _decode(decode)
//     {}

//     auto encode();

// private:
//     Encode _encode;
//     Decode _decode;
// };


template <class T, T... Values>
struct LookUpTableTransform {
    static constexpr size_t size = sizeof...(Values);
    using output_type = T;
    using input_type = uint_t<cilog2(size)>;
    using backward_map_map_type = std::unordered_map<T, input_type>;
    static constexpr std::array<T, size> forward_map = {Values...};
    static const backward_map_map_type backward_map;

    template <size_t... Is>
    static auto init_backward_map(std::index_sequence<Is...>) {
        return backward_map_map_type({{forward_map[Is], Is}...});
    }

    static auto init_backward_map() {
        return init_backward_map(std::make_index_sequence<size>());
    }

    output_type encode(const input_type& value) const {
        return forward_map[value];
    }

    input_type decode(const output_type& value) const {
        auto it = backward_map.find(value);
        if (it == backward_map.end())
            return {};
        else
            return it->second;
    }
};

template <class LUT, size_t... Is>
auto make_map(std::index_sequence<Is...>) {
    return LUT::backward_map_map_type({LUT::values[Is], Is}...);
}

template <class T, T... Values>
const typename LookUpTableTransform<T, Values...>::backward_map_map_type 
LookUpTableTransform<T, Values...>::backward_map = LookUpTableTransform<T, Values...>::init_backward_map();

}

#endif