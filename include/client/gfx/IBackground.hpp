#ifndef IBACKGROUND_HPP
#define IBACKGROUND_HPP

#include "IWindow.hpp"

namespace rtype::engine {
  // Anything drawn behind the game entities (starfield, planets, scrolling level...).
  class IBackground {
  public:
    virtual ~IBackground() = default;

    virtual void update(float deltaTime) = 0;
    virtual void render(IWindow& window) = 0;
  };
}

#endif // !IBACKGROUND_HPP
