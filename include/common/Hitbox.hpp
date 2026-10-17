#ifndef HITBOX_HPP
#define HITBOX_HPP

namespace rtype::game {
  struct Hitbox {
    float x;
    float y;
    float width;
    float height;

    constexpr Hitbox movedTo(float posX, float posY) const {
      return Hitbox{posX + x, posY + y, width, height};
    }

    constexpr bool intersects(const Hitbox& other) const {
      return x < other.x + other.width && other.x < x + width
          && y < other.y + other.height && other.y < y + height;
    }
  };
}

#endif // !HITBOX_HPP
