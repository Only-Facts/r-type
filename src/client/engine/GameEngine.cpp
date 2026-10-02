#include "GameEngine.hpp"

namespace rtype::engine {
  GameEngine::GameEngine(std::unique_ptr<IWindow> window,
                         std::unique_ptr<IInputManager> inputManager,
                         std::unique_ptr<ITextureManager> textureManager)
    : _window(std::move(window)), _inputManager(std::move(inputManager)), _textureManager(std::move(textureManager)) {}

  void GameEngine::run() {
    while (_window->isOpen()) {
      _inputManager->update();
      _window->pollEvents(*_inputManager);

      float dt = _window->getDeltaTime();

      update(dt);
      render();
    }
  }

  void GameEngine::update(float deltaTime) {
    (void)deltaTime;
    if (_inputManager->isKeyJustPressed(KeyCode::Escape)) _window->close();
  }

  void GameEngine::render() {
    _window->clear();
    _window->display();
  }
}
