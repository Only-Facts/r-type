#ifndef RTYPE_ENGINE_GAMEENGINE_HPP
#define RTYPE_ENGINE_GAMEENGINE_HPP

#include "engine/audio/AudioService.hpp"
#include "engine/config/Config.hpp"
#include "engine/core/Clock.hpp"
#include "engine/core/EngineContext.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include "engine/input/ActionMap.hpp"
#include "engine/platform/IBackend.hpp"
#include "engine/platform/WindowConfig.hpp"
#include "engine/resources/ResourceManager.hpp"
#include "engine/scene/SceneManager.hpp"
#include <memory>
#include <string>

namespace rtype::engine {
  /// Entry point of the engine. Everything is driven by the config:
  ///   [engine]  backend, asset manifest, preload, quit/mute actions, clear color
  ///   [window]  width, height, title, framerate, vsync
  ///   [input]   action -> keys bindings
  ///   [audio]   enabled, muted, master/music/sfx volumes
  ///   [debug]   overlay font, toggle action, initial visibility
  /// The game only registers scenes and calls run().
  class GameEngine {
  public:
    explicit GameEngine(Config config);
    ~GameEngine() = default;

    GameEngine(const GameEngine&) = delete;
    GameEngine& operator=(const GameEngine&) = delete;

    void run();
    void stop();

    EngineContext& getContext() { return _context; }
    SceneManager& getSceneManager() { return _sceneManager; }
    ResourceManager& getResources() { return _resources; }
    AudioService& getAudio() { return _audio; }
    ActionMap& getActions() { return _actions; }
    const Config& getConfig() const { return _config; }
    const std::string& getBackendName() const { return _backendName; }

  private:
    void update(float dt);
    void render();
    void updateStats(float dt);

    static WindowConfig makeWindowConfig(const Config& config);
    static std::unique_ptr<IBackend> makeBackend(const std::string& name, const WindowConfig& window);

    Config _config;
    std::string _backendName;
    std::unique_ptr<IBackend> _backend;
    ResourceManager _resources;
    AudioService _audio;
    ActionMap _actions;
    SceneManager _sceneManager;
    Clock _clock;
    Color _clearColor;
    std::string _quitAction;
    std::string _muteAction;
    std::string _debugAction;
    FrameStats _stats;
    DebugState _debug;
    float _statsElapsed{0.0f};
    int _statsFrames{0};
    EngineContext _context;
  };
}

#endif // !RTYPE_ENGINE_GAMEENGINE_HPP
