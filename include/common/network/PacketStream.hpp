#ifndef PACKETSTREAM_HPP
#define PACKETSTREAM_HPP

#include "Packet.hpp"
#include "Protocol.hpp"
#include "exception.hpp"
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace rtype::network {
  class PacketStreamWriter {
  public:
    PacketStreamWriter() = default;

    template<typename T>
    PacketStreamWriter& operator<<(const T& data) {
      static_assert(std::is_trivially_copyable_v<T>,
                    "Type must be trivially copyable");
      if constexpr (std::is_same_v<T, bool>) {
        _buffer.push_back(data ? 1u : 0u);
      } else if constexpr (std::is_enum_v<T>) {
        using Value = std::underlying_type_t<T>;
        *this << static_cast<Value>(data);
      } else if constexpr (std::is_integral_v<T>) {
        using Value = std::make_unsigned_t<T>;
        const Value value = static_cast<Value>(data);
        for (std::size_t i = 0; i < sizeof(T); ++i) {
          _buffer.push_back(static_cast<std::uint8_t>(
            (value >> (i * 8)) & static_cast<Value>(0xFFu)));
        }
      } else if constexpr (std::is_floating_point_v<T>) {
        static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                      "Only IEEE-754 float and double are supported");
        if constexpr (sizeof(T) == 4)
          *this << std::bit_cast<std::uint32_t>(data);
        else
          *this << std::bit_cast<std::uint64_t>(data);
      } else {
        const auto* ptr = reinterpret_cast<const std::uint8_t*>(&data);
        _buffer.insert(_buffer.end(), ptr, ptr + sizeof(T));
      }
      return *this;
    }

    PacketStreamWriter& operator<<(const PacketHeader& header) {
      const std::uint16_t magic = header.magic;
      const std::uint8_t type = static_cast<std::uint8_t>(header.type);
      const std::uint32_t sequence = header.sequence;
      const std::uint16_t payloadSize = header.payloadSize;
      *this << magic << type << sequence << payloadSize;
      return *this;
    }

    PacketStreamWriter& operator<<(const EntityStateData& state) {
      *this << state.netId << state.posX << state.posY
            << state.velX << state.velY;
      return *this;
    }

    PacketStreamWriter& operator<<(const PackedEntityStateData& state) {
      *this << state.netId << state.posX << state.posY
            << state.velX << state.velY;
      return *this;
    }

    PacketStreamWriter& operator<<(const std::string& str) {
      if (str.size() > std::numeric_limits<std::uint16_t>::max())
        throw RtypeError("PacketStreamWriter: string is too large");
      const auto size = static_cast<std::uint16_t>(str.size());
      *this << size;
      _buffer.insert(_buffer.end(), str.begin(), str.end());
      return *this;
    }

    [[nodiscard]] const PacketData& getBuffer() const { return _buffer; }

  private:
    PacketData _buffer;
  };

  class PacketStreamReader {
  public:
    explicit PacketStreamReader(const PacketData& data)
      : _buffer(data), _readOffset(0) {}

    template<typename T>
    PacketStreamReader& operator>>(T& data) {
      static_assert(std::is_trivially_copyable_v<T>,
                    "Type must be trivially copyable");
      if constexpr (std::is_same_v<T, bool>) {
        std::uint8_t value{};
        readBytes(&value, sizeof(value));
        if (value > 1) throw RtypeError("PacketStreamReader: invalid bool");
        data = value != 0;
      } else if constexpr (std::is_enum_v<T>) {
        using Value = std::underlying_type_t<T>;
        Value value{};
        *this >> value;
        data = static_cast<T>(value);
      } else if constexpr (std::is_integral_v<T>) {
        using Value = std::make_unsigned_t<T>;
        Value value{};
        for (std::size_t i = 0; i < sizeof(T); ++i) {
          std::uint8_t byte{};
          readBytes(&byte, sizeof(byte));
          value |= static_cast<Value>(byte) << (i * 8);
        }
        if constexpr (std::is_signed_v<T>)
          data = std::bit_cast<T>(value);
        else
          data = value;
      } else if constexpr (std::is_floating_point_v<T>) {
        static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                      "Only IEEE-754 float and double are supported");
        if constexpr (sizeof(T) == 4) {
          std::uint32_t value{};
          *this >> value;
          data = std::bit_cast<T>(value);
        } else {
          std::uint64_t value{};
          *this >> value;
          data = std::bit_cast<T>(value);
        }
      } else {
        readBytes(&data, sizeof(T));
      }
      return *this;
    }

    PacketStreamReader& operator>>(PacketHeader& header) {
      std::uint16_t magic{};
      std::uint8_t type{};
      std::uint32_t sequence{};
      std::uint16_t payloadSize{};
      *this >> magic >> type >> sequence >> payloadSize;
      header.magic = magic;
      header.type = static_cast<PacketType>(type);
      header.sequence = sequence;
      header.payloadSize = payloadSize;
      return *this;
    }

    PacketStreamReader& operator>>(EntityStateData& state) {
      *this >> state.netId >> state.posX >> state.posY
            >> state.velX >> state.velY;
      return *this;
    }

    PacketStreamReader& operator>>(PackedEntityStateData& state) {
      *this >> state.netId >> state.posX >> state.posY
            >> state.velX >> state.velY;
      return *this;
    }

    PacketStreamReader& operator>>(std::string& str) {
      std::uint16_t size = 0;
      *this >> size;
      if (size > _buffer.size() - _readOffset)
        throw RtypeError("PacketStreamReader: invalid string length");
      str.assign(reinterpret_cast<const char*>(_buffer.data() + _readOffset),
                 size);
      _readOffset += size;
      return *this;
    }

    [[nodiscard]] bool hasMoreData() const {
      return _readOffset < _buffer.size();
    }
    [[nodiscard]] std::size_t remaining() const {
      return _buffer.size() - _readOffset;
    }

  private:
    void readBytes(void* destination, std::size_t size) {
      if (size > _buffer.size() - _readOffset)
        throw RtypeError("PacketStreamReader: packet is truncated");
      std::memcpy(destination, _buffer.data() + _readOffset, size);
      _readOffset += size;
    }

    const PacketData& _buffer;
    std::size_t _readOffset{0};
  };
}

#endif // !PACKETSTREAM_HPP
