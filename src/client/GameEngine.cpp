#include "GameEngine.hpp"

namespace rtype::engine {
  GameEngine::GameEngine(std::unique_ptr<IWindow> window) 
    : _window(std::move(window)) {}

  void GameEngine::run() {
    while (_window->isOpen()) {
      _window->pollEvents();

      float dt = _window->getDeltaTime();

      update(dt);
      render();
    }
  }

  void GameEngine::update(float deltaTime) {
    (void)deltaTime; 
  }

  void GameEngine::render() {
    _window->clear();
    _window->display();
  }
}
