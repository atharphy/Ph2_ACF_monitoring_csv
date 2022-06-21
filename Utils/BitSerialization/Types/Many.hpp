#ifndef BITSERIALIZATION__TYPES__MANY_HPP
#define BITSERIALIZATION__TYPES__MANY_HPP

#include "../Core.hpp"

namespace BitSerialization {
    
template <class Type, size_t MinSize = 0>
struct Many {
    static constexpr bool ignores_input_value = ignores_input_value<Type>;
    using value_type = ConvertibleVector<value_type_t<Type>>;


    struct ManySizeError {
        size_t size;

        friend std::ostream& operator<<(std::ostream& os, const ManySizeError& error) {
            return (os << "Many error: not enough elements (" << error.size << " / " << MinSize << ").");
        }
    };

    using ManyElementError = ElementError<parse_error_t<Type>, "Many">;

    using ParseError = ErrorVariant<ManyElementError, ManySizeError>;

    template <class T, class U=Void>
    static ParseResult<value_type, ParseError> 
    parse(const BitView<T>& bits, const U& parent={}) {
        value_type results;
        size_t offset = 0;
        while (offset < bits.size()) {
            auto result = Type::parse(bits.slice(offset), parent);
            if (!result) {
                if (results.size() < MinSize)
                    return {ManyElementError(results.size(), std::move(result.error()))};
                break;
            }
            results.push_back(std::move(result.value()));
            offset += result.size();
        }
        if (results.size() < MinSize)
            return {ManySizeError(results.size())};
        return {std::move(results), offset};
    }

    using SerializeError = ElementError<serialize_error_t<Type>, "Many">;

    template <class T, class U=Void>
    static SerializeResult<SerializeError>
    serialize(value_type& value, BitVector<T>& bits, const U& parent={}) {
        for (size_t i = 0; i < value.size(); ++i) {
            auto result = Type::serialize(value[i], bits, parent);
            if (!result) 
                return SerializeError{i, std::move(result.error())};
        }
        return {};
    }
};

}

#endif