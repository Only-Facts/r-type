#ifndef RTYPE_ENGINE_SCENEMANAGER_HPP
#define RTYPE_ENGINE_SCENEMANAGER_HPP

#include "engine/platform/IWindow.hpp"
#include "engine/scene/IScene.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace rtype::engine {
  class SceneManager {
  public:
    using Factory = std::function<std::shared_ptr<IScene>()>;

    SceneManager() = default;
    ~SceneManager() = default;

    void addScene(const std::string& name, std::shared_ptr<IScene> scene);
    // the factory builds a fresh scene every time the scene is entered (e.g. a new game)
    void addScene(const std::string& name, Factory factory);
    bool hasScene(const std::string& name) const;
    void changeScene(const std::string& name);

    void update(float dt);
    void render(IWindow& window);

  private:
    std::unordered_map<std::string, std::shared_ptr<IScene>> _scenes;
    std::unordered_map<std::string, Factory> _factories;
    std::shared_ptr<IScene> _currentScene{nullptr};
    // the scene left during this frame: kept alive until the next update, since it may still be running
    std::shared_ptr<IScene> _previousScene{nullptr};
  };
}

#endif // !RTYPE_ENGINE_SCENEMANAGER_HPP
