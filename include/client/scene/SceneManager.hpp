#ifndef SCENEMANAGER_HPP
#define SCENEMANAGER_HPP

#include "IScene.hpp"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace rtype::engine {
  class SceneManager {
  public:
    SceneManager() = default;
    ~SceneManager() = default;

    void addScene(const std::string& name, std::shared_ptr<IScene> scene);
    void removeScene(const std::string& name);

    void changeScene(const std::string& name);

    void requestQuit();
    bool isQuitRequested() const;

    void update(float dt);
    void render(IWindow& window);

  private:
    void applyPendingScene();

    std::unordered_map<std::string, std::shared_ptr<IScene>> _scenes;
    std::shared_ptr<IScene> _currentScene{nullptr};
    std::optional<std::string> _pendingScene;
    bool _isQuitRequested{false};
  };
}

#endif // !SCENEMANAGER_HPP
