#include "scene/SceneManager.hpp"
#include "exception.hpp"

namespace rtype::engine {
  void SceneManager::addScene(const std::string& name, std::shared_ptr<IScene> scene) {
    _scenes[name] = scene;
  }

  void SceneManager::removeScene(const std::string& name) {
    _scenes.erase(name);
  }

  void SceneManager::changeScene(const std::string& name) {
    if (_scenes.find(name) == _scenes.end()) throw RtypeError("Scene not found: " + name);
    _pendingScene = name;
  }

  void SceneManager::requestQuit() {
    _isQuitRequested = true;
  }

  bool SceneManager::isQuitRequested() const {
    return _isQuitRequested;
  }

  void SceneManager::update(float dt) {
    applyPendingScene();
    if (_currentScene) _currentScene->update(dt);
  }

  void SceneManager::render(IWindow& window) {
    if (_currentScene) _currentScene->render(window);
  }

  void SceneManager::applyPendingScene() {
    if (!_pendingScene) return;

    auto it = _scenes.find(*_pendingScene);
    _pendingScene.reset();
    if (it == _scenes.end()) return;

    _currentScene = it->second;
    _currentScene->init();
  }
}
