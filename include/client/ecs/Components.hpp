#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

#include "gfx/GfxTypes.hpp"
#include "gfx/ISprite.hpp"
#include <memory>

namespace rtype::engine {
  struct TransformComponent {
    Vector2f position{0.0f, 0.0f};
    float rotation{0.0f};
    Vector2f scale{1.0f, 1.0f};
  };

  struct VelocityComponent {
    Vector2f velocity{0.0f, 0.0f};
  };

  struct SpriteComponent {
    std::shared_ptr<ISprite> sprite{nullptr};
  };
}

#endif // !COMPONENTS_HPP
