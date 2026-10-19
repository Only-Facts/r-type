#ifndef RTYPE_ENGINE_ENGINECONTEXT_HPP
#define RTYPE_ENGINE_ENGINECONTEXT_HPP

#include "engine/core/FrameStats.hpp"

namespace rtype::engine {
  class ActionMap;
  class AudioService;
  class Config;
  class IBackend;
  class IInputManager;
  class IWindow;
  class ResourceManager;
  class SceneManager;
  class Session;

  struct EngineContext {
    const Config& config;
    IWindow& window;
    IInputManager& input;
    ActionMap& actions;
    ResourceManager& resources;
    AudioService& audio;
    SceneManager& scenes;
    IBackend& backend;
    const FrameStats& stats;
    DebugState& debug;
    Session& session;
  };
}

#endif // !RTYPE_ENGINE_ENGINECONTEXT_HPP
