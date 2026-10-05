#ifndef ISCENE_HPP
#define ISCENE_HPP

#include "IWindow.hpp"

namespace rtype::engine {
  class IScene {
  public:
    virtual ~IScene() = default;

    virtual void init() = 0;
    virtual void update(float dt) = 0;
    virtual void render(IWindow& window) = 0;
  };
}

#endif // !ISCENE_HPP
