#include "engine/scene/SceneManager.hpp"
#include "exception.hpp"
#include <utility>

namespace rtype::engine {
  void SceneManager::addScene(const std::string& name, std::shared_ptr<IScene> scene) {
    if (!scene) throw RtypeError("Cannot register a null scene: " + name);
    _scenes[name] = std::move(scene);
  }

  bool SceneManager::hasScene(const std::string& name) const {
    return _scenes.find(name) != _scenes.end();
  }

  void SceneManager::changeScene(const std::string& name) {
    auto it = _scenes.find(name);
    if (it == _scenes.end()) throw RtypeError("Scene not found: " + name);
    if (_currentScene == it->second) return;

    if (_currentScene) _currentScene->exit();
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
