#ifndef SCENEMANAGER_HPP
#define SCENEMANAGER_HPP

#include "IScene.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace rtype::engine {
  class SceneManager {
  public:
    SceneManager() = default;
    ~SceneManager() = default;

    void addScene(const std::string& name, std::shared_ptr<IScene> scene);
    void changeScene(const std::string& name);

    void update(float dt);
    void render(IWindow& window);

  private:
    std::unordered_map<std::string, std::shared_ptr<IScene>> _scenes;
    std::shared_ptr<IScene> _currentScene{nullptr};
  };
}

#endif // !SCENEMANAGER_HPP
