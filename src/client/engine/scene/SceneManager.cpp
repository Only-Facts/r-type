#include "scene/SceneManager.hpp"
#include "exception.hpp"

namespace rtype::engine {
  void SceneManager::addScene(const std::string& name, std::shared_ptr<IScene> scene) {
    _scenes[name] = scene;
  }

  void SceneManager::changeScene(const std::string& name) {
    auto it = _scenes.find(name);
    if (it == _scenes.end()) throw RtypeError("Scene not found: " + name);
    _currentScene = it->second;
    _currentScene->init();
  }

  void SceneManager::update(float dt) {
    if (_currentScene) _currentScene->update(dt);
  }

  void SceneManager::render(IWindow& window) {
    if (_currentScene) _currentScene->render(window);
  }
}
