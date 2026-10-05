#ifndef GAMEENGINE_HPP
#define GAMEENGINE_HPP

#include "IWindow.hpp"
#include "gfx/ITextureManager.hpp"
#include "input/IInputManager.hpp"
#include "scene/SceneManager.hpp"
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

    SceneManager& getSceneManager() { return _sceneManager; }
    ITextureManager& getTextureManager() { return *_textureManager; }
    IInputManager& getInputManager() { return *_inputManager; }

  private:
    std::unique_ptr<IWindow> _window;
    std::unique_ptr<IInputManager> _inputManager;
    std::unique_ptr<ITextureManager> _textureManager;
    SceneManager _sceneManager;
  };
}

#endif // !GAMEENGINE_HPP
