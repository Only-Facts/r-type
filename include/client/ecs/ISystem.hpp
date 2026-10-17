#ifndef ISYSTEM_HPP
#define ISYSTEM_HPP

#include "Registry.hpp"

namespace rtype::engine {
  class ISystem {
  public:
    virtual ~ISystem() = default;

    virtual void update(Registry& registry, float deltaTime) = 0;
  };
}

#endif // !ISYSTEM_HPP
