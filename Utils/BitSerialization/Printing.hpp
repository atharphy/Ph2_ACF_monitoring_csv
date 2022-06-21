#ifndef BITSERIALIZATION__PRINTING_HPP
#define BITSERIALIZATION__PRINTING_HPP

#include <ostream>
#include <utility>
#include <vector>
#include <array>
#include <optional>

namespace BitSerialization {
    

/* Helper function to get a storage index in a stream */
inline int get_indent_index() {
    /* ios_base::xalloc allocates indices for custom-storage locations. These indices are valid for all streams */
    static int index = std::ios_base::xalloc();
    return index;
}

inline std::ios_base& increase_indent(std::ios_base& stream) {
    /* The iword(index) function gives a reference to the index-th custom storage location as a integer */
    stream.iword(get_indent_index())++;
    return stream;
}

inline std::ios_base& decrease_indent(std::ios_base& stream) {
    /* The iword(index) function gives a reference to the index-th custom storage location as a integer */
    stream.iword(get_indent_index())--;
    return stream;
}


// template<class charT, class traits>
inline std::ostream& endl_indent(std::ostream& stream) {
    int indent = stream.iword(get_indent_index());
    stream.put(stream.widen('\n'));
    while (indent) {
        for (int i = 0; i < 4; ++i)
            stream.put(stream.widen(' '));
        indent--;
    }
    stream.flush();
    return stream;
}


template <class T, typename std::enable_if_t<!std::is_const_v<T>, int> = 0>
inline std::ostream& operator<<(std::ostream& os, std::reference_wrapper<T> ref) {
    return (os << std::reference_wrapper<const T>(ref.get()));
}


template <class T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, uint8_t>, int> = 0>
inline std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const T> wrapper)
{
    os << wrapper.get();
    return os;
}

inline std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const uint8_t> wrapper)
{
    os << +wrapper.get();
    return os;
}

template <class Container>
inline std::ostream& print_container(std::ostream& os, const Container& container)
{
    if (container.size() == 0)
        return (os << "[]");
    os << "[" << increase_indent << endl_indent;
    auto it = container.begin();
    os << std::ref(*it);
    for (++it; it < container.end(); ++it) {
        os << "," << endl_indent << std::ref(*it);
    }
    os << decrease_indent << endl_indent << "]";
    return os;
}

template <class T>
inline std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const std::vector<T>> wrapper)
{
    return print_container(os, wrapper.get());
}

template <class T>
inline std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const ConvertibleVector<T>> wrapper)
{
    return print_container(os, wrapper.get());
}

template <class T, size_t N>
inline std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const std::array<T, N>> wrapper)
{
    return print_container(os, wrapper.get());
}

template <class T>
inline std::ostream& operator<<(std::ostream& os, std::reference_wrapper<const std::optional<T>> wrapper)
{
    auto& opt = wrapper.get();
    if (opt)
        return (os << std::ref(opt.value()));
    else
        return (os << "N/A");
}


}

#endif