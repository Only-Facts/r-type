#include "engine/scene/SceneManager.hpp"
#include "exception.hpp"
#include <utility>

namespace rtype::engine {
  void SceneManager::addScene(const std::string& name, std::shared_ptr<IScene> scene) {
    if (!scene) throw RtypeError("Cannot register a null scene: " + name);
    _factories.erase(name);
    _scenes[name] = std::move(scene);
  }

  void SceneManager::addScene(const std::string& name, Factory factory) {
    if (!factory) throw RtypeError("Cannot register a null scene factory: " + name);
    _scenes.erase(name);
    _factories[name] = std::move(factory);
  }

  bool SceneManager::hasScene(const std::string& name) const {
    return _scenes.find(name) != _scenes.end() || _factories.find(name) != _factories.end();
  }

  void SceneManager::changeScene(const std::string& name) {
    std::shared_ptr<IScene> next;
    if (auto factory = _factories.find(name); factory != _factories.end()) {
      next = factory->second();
      if (!next) throw RtypeError("Scene factory returned null: " + name);
    } else if (auto it = _scenes.find(name); it != _scenes.end()) {
      next = it->second;
    } else {
      throw RtypeError("Scene not found: " + name);
    }
    if (_currentScene == next) return;

    if (_currentScene) _currentScene->exit();
    _previousScene = std::move(_currentScene);
    _currentScene = std::move(next);
    _currentScene->init();
  }

  void SceneManager::update(float dt) {
    _previousScene.reset();
    if (_currentScene) _currentScene->update(dt);
  }

  void SceneManager::render(IWindow& window) {
    if (_currentScene) _currentScene->render(window);
  }
}
