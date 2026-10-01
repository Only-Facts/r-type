#ifndef GAMEENGINE_HPP
#define GAMEENGINE_HPP

#include "IWindow.hpp"
#include "input/IInputManager.hpp"
#include <memory>

namespace rtype::engine {
  class GameEngine {
  public:
    GameEngine(std::unique_ptr<IWindow> window, std::unique_ptr<IInputManager> inputManager);
    ~GameEngine() = default;

    void run();

  private:
    std::unique_ptr<IWindow> _window;
    std::unique_ptr<IInputManager> _inputManager;

    void update(float deltaTime);
    void render();
  };
}

#endif // !GAMEENGINE_HPP
