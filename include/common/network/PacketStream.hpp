#ifndef PACKETSTREAM_HPP
#define PACKETSTREAM_HPP

#include "Packet.hpp"
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include "exception.hpp"

namespace rtype::network {
  class PacketStreamWriter {
  public:
    PacketStreamWriter() = default;

    template<typename T>
    PacketStreamWriter& operator<<(const T& data) {
      static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");

      const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(&data);
      _buffer.insert(_buffer.end(), ptr, ptr + sizeof(T));
      return *this;
    }

    PacketStreamWriter& operator<<(const std::string& str) {
      auto size = static_cast<std::uint16_t>(str.size());
      *this << size;

      _buffer.insert(_buffer.end(), str.begin(), str.end());
      return *this;
    }

    const PacketData& getBuffer() const { return _buffer; }

  private:
    PacketData _buffer;
  };

  class PacketStreamReader {
  public:
    explicit PacketStreamReader(const PacketData& data) : _buffer(data), _readOffset(0) {}

    template<typename T>
    PacketStreamReader& operator>>(T& data) {
      static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
      if (_readOffset + sizeof(T) > _buffer.size()) throw RtypeError("PacketStreamReader overflow: trying to read beyond packet size");

      std::memcpy(&data, _buffer.data() + _readOffset, sizeof(T));
      _readOffset += sizeof(T);
      return *this;
    }

    PacketStreamReader& operator>>(std::string& str) {
      std::uint16_t size = 0;
      *this >> size;
      if (_readOffset + size > _buffer.size()) throw RtypeError("PacketStreamReader overflow: invalid string length");

      str.assign(reinterpret_cast<const char*>(_buffer.data() + _readOffset), size);
      _readOffset += size;
      return *this;
    }

    [[nodiscard]] bool hasMoreData() const { return _readOffset < _buffer.size(); }

  private:
    const PacketData& _buffer;
    std::size_t _readOffset{0};
  };
}

#endif // !PACKETSTREAM_HPP
