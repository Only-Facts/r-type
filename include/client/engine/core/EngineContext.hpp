#ifndef RTYPE_ENGINE_ENGINECONTEXT_HPP
#define RTYPE_ENGINE_ENGINECONTEXT_HPP

namespace rtype::engine {
  class ActionMap;
  class AudioService;
  class Config;
  class IBackend;
  class IInputManager;
  class IWindow;
  class ResourceManager;
  class SceneManager;

  /// Everything a scene may ask the engine for. Only interfaces and engine
  /// services: a scene can never reach a backend-specific type.
  struct EngineContext {
    const Config& config;
    IWindow& window;
    IInputManager& input;
    ActionMap& actions;
    ResourceManager& resources;
    AudioService& audio;
    SceneManager& scenes;
    IBackend& backend;
  };
}

#endif // !RTYPE_ENGINE_ENGINECONTEXT_HPP
