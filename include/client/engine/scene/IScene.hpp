#ifndef RTYPE_ENGINE_ISCENE_HPP
#define RTYPE_ENGINE_ISCENE_HPP

#include "engine/platform/IWindow.hpp"

namespace rtype::engine {
  class IScene {
  public:
    virtual ~IScene() = default;

    virtual void init() = 0;
    virtual void update(float dt) = 0;
    virtual void render(IWindow& window) = 0;
    virtual void exit() {}
  };
}

#endif // !RTYPE_ENGINE_ISCENE_HPP
