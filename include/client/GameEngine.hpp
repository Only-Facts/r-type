#ifndef GAMEENGINE_HPP
#define GAMEENGINE_HPP

#include "IWindow.hpp"
#include <memory>

namespace rtype::engine {
  class GameEngine {
  public:
    GameEngine(std::unique_ptr<IWindow> window);
    ~GameEngine() = default;

    void run();

  private:
    std::unique_ptr<IWindow> _window;
    void update(float deltaTime);
    void render();
  };
}

#endif // !GAMEENGINE_HPP
