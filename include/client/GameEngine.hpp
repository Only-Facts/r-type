#ifndef GAMEENGINE_HPP
#define GAMEENGINE_HPP

#include "IWindow.hpp"
#include "gfx/ITextureManager.hpp"
#include "input/IInputManager.hpp"
#include <memory>

namespace rtype::engine {
  class GameEngine {
  public:
    GameEngine(std::unique_ptr<IWindow> window,
               std::unique_ptr<IInputManager> inputManager,
               std::unique_ptr<ITextureManager> textureManager);
    ~GameEngine() = default;

    void init();
    void update(float dt);
    void render();
    void run();

  private:
    std::unique_ptr<IWindow> _window;
    std::unique_ptr<IInputManager> _inputManager;
    std::unique_ptr<ITextureManager> _textureManager;
  };
}

#endif // !GAMEENGINE_HPP
