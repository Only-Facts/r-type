#ifndef RTYPE_ENGINE_ISYSTEM_HPP
#define RTYPE_ENGINE_ISYSTEM_HPP

#include "engine/ecs/Registry.hpp"

namespace rtype::engine {
  class ISystem {
  public:
    virtual ~ISystem() = default;
    virtual void update(Registry& registry, float dt) = 0;
  };
}

#endif // !RTYPE_ENGINE_ISYSTEM_HPP
