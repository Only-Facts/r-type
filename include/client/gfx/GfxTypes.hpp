#ifndef GFXTYPES_HPP
#define GFXTYPES_HPP

#include <cstdint>

namespace rtype::engine {
  struct Vector2f {
    float x{0.0f};
    float y{0.0f};
  };

  struct Vector2u {
    unsigned int x{0};
    unsigned int y{0};
  };

  struct IntRect {
    int left{0};
    int top{0};
    int width{0};
    int height{0};
  };

  struct Color {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t a{255};
  };
}

#endif // !GFXTYPES_HPP
