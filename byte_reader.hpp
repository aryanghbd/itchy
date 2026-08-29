#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

class ByteReader {
private:
    std::span<const uint8_t> m_bytes;
    std::size_t m_position = 0;

    void require(std::size_t count) const {
        if (count > m_bytes.size() - m_position) {
            throw std::runtime_error("Unexpected end of message");
        }
    }

public:
    explicit ByteReader(std::span<const uint8_t> bytes)
        : m_bytes(bytes) {}

    uint8_t readUInt8() {
        require(1);
        return m_bytes[m_position++];
    }

    uint16_t readUInt16BE() {
        require(2);
        uint16_t value =
            (static_cast<uint16_t>(m_bytes[m_position]) << 8) |
            static_cast<uint16_t>(m_bytes[m_position + 1]);
        m_position += 2;
        return value;
    }

    uint32_t readUInt32BE() {
        require(4);
        uint32_t value =
            (static_cast<uint32_t>(m_bytes[m_position]) << 24) |
            (static_cast<uint32_t>(m_bytes[m_position + 1]) << 16) |
            (static_cast<uint32_t>(m_bytes[m_position + 2]) << 8) |
            static_cast<uint32_t>(m_bytes[m_position + 3]);
        m_position += 4;
        return value;
    }

    uint64_t readUInt48BE() {
        require(6);
        uint64_t value = 0;
        for (std::size_t i = 0; i < 6; ++i) {
            value = (value << 8) | m_bytes[m_position + i];
        }
        m_position += 6;
        return value;
    }

    uint64_t readUInt64BE() {
        require(8);
        uint64_t value = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            value = (value << 8) | m_bytes[m_position + i];
        }
        m_position += 8;
        return value;
    }

    void readChars(char* destination, std::size_t count) {
        require(count);
        for (std::size_t i = 0; i < count; ++i) {
            destination[i] = static_cast<char>(m_bytes[m_position + i]);
        }
        m_position += count;
    }
};
