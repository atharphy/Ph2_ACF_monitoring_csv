#ifndef BITS__BIT_VECTOR_HPP
#define BITS__BIT_VECTOR_HPP

#include "BitView.hpp"


template <class BlockType>
class BitVector {
    static constexpr size_t block_size = 8 * sizeof(BlockType);
public:

    using block_iterator = typename std::vector<BlockType>::iterator;

    BitVector() : _data(), _size(0) {}

    template <class C>
    BitVector(C&& c, size_t size)
     : _data(std::forward<C>(c))
     , _size(size)
    {}

    size_t size() const { return _size; }

    BitView<BlockType> append_zeros(size_t n) {
        int extra_bits = n + _size - _data.size() * block_size;
        if (extra_bits > 0) {
            int extra_words = (extra_bits + block_size - 1) / block_size;
            _data.insert(std::end(_data), extra_words, 0);
        }
        size_t old_size = _size;
        _size += n;
        return {_data.data(), old_size, _size};
    }

    template <class BlockTypeOther>
    void append(const BitView<BlockTypeOther>& bits) {
        int extra_bits = bits.size() + _size - _data.size() * block_size;
        
        if (extra_bits > 0) {
            int extra_words = (extra_bits + block_size - 1) / block_size;
            _data.insert(std::end(_data), extra_words, 0);
        }
        auto new_bits = BitView<BlockType>{_data.data(), _size, _size + bits.size()};
        size_t offset = 0;
        while (offset + 8 < bits.size()) {
            uint8_t byte = bits.slice(offset, offset + 8).template get<uint8_t>();
            new_bits.slice(offset, offset + 8).set(byte);
            offset += 8;
        }
        int leftover_bits = bits.size() - offset;
        if (leftover_bits > 0) {
            uint8_t byte = bits.slice(offset, offset + leftover_bits).template get<uint8_t>();
            new_bits.slice(offset, offset + leftover_bits).set(byte);
        }
        _size += bits.size();
    }

    block_iterator erase_blocks(block_iterator it) {
        _size = std::max(_size - block_size, 0ul);
        return _data.erase(it);
    }

    block_iterator erase_blocks(block_iterator first, block_iterator last) {
        _size = std::max(_size - block_size * (last - first), 0ul);
        return _data.erase(first, last);
    }

    void clear() {
        _data.clear();
        _size = 0;
    }

    operator BitView<BlockType>() {
        return {_data.data(), 0, _size};
    }

    operator BitView<const BlockType>() const {
        return {_data.data(), 0, _size};
    }

    BitView<BlockType> view() {
        return {_data.data(), 0, _size};
    }

    
    BitView<const BlockType> view() const {
        return {_data.data(), 0, _size};
    }

    std::vector<BlockType>& blocks() { return _data; }
    const std::vector<BlockType>& blocks() const { return _data; }

private:
    std::vector<BlockType> _data;
    size_t _size;
};

template <class T>
BitView<T> bit_view(BitVector<T>& vec) {
    return vec;
}

template <class T>
BitView<const T> bit_view(const BitVector<T>& vec) {
    return vec;
}

#endif