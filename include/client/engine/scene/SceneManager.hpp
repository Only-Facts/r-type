#ifndef RTYPE_ENGINE_SCENEMANAGER_HPP
#define RTYPE_ENGINE_SCENEMANAGER_HPP

#include "engine/platform/IWindow.hpp"
#include "engine/scene/IScene.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace rtype::engine {
  class SceneManager {
  public:
    SceneManager() = default;
    ~SceneManager() = default;

    void addScene(const std::string& name, std::shared_ptr<IScene> scene);
    bool hasScene(const std::string& name) const;
    void changeScene(const std::string& name);

    void update(float dt);
    void render(IWindow& window);

  private:
    std::unordered_map<std::string, std::shared_ptr<IScene>> _scenes;
    std::shared_ptr<IScene> _currentScene{nullptr};
  };
}

#endif // !RTYPE_ENGINE_SCENEMANAGER_HPP
