#include "engine/GameEngine.hpp"
#include "backend/Backends.hpp"
#include "engine/platform/BackendFactory.hpp"
#include "exception.hpp"

namespace rtype::engine {
  GameEngine::GameEngine(Config config)
    : _config(std::move(config)),
      _backendName(_config.getString("engine", "backend", "sfml")),
      _backend(makeBackend(_backendName, makeWindowConfig(_config))),
      _resources(_backend->getTextures(), _backend->getFonts(), _backend->getAudio()),
      _audio(_backend->getAudio(), _resources),
      _actions(_backend->getInput()),
      _clock(_config.getFloat("engine", "max_frame_time", 0.25f)),
      _clearColor(_config.getColor("engine", "clear_color", Color{0, 0, 0, 255})),
      _quitAction(_config.getString("engine", "quit_action", "")),
      _muteAction(_config.getString("engine", "mute_action", "")),
      _debugAction(_config.getString("debug", "toggle_action", "")),
      _context{_config, _backend->getWindow(), _backend->getInput(), _actions, _resources, _audio,
               _sceneManager, *_backend, _stats, _debug, _session} {
    _debug.overlayVisible = _config.getBool("debug", "visible", false);
    _actions.loadFromConfig(_config, "input");
    _audio.loadSettings(_config, "audio");

    const std::string manifest = _config.getString("engine", "asset_manifest");
    if (!manifest.empty()) _resources.loadManifestFile(manifest);
    if (_config.getBool("engine", "preload_assets", false)) _resources.preloadAll();
  }

  void GameEngine::run() {
    IWindow& window = _backend->getWindow();
    IInputManager& input = _backend->getInput();

    _clock.reset();
    while (window.isOpen()) {
      input.update();
      _backend->pollEvents();
      if (!window.isOpen()) break;

      const float dt = _clock.tick();
      updateStats(dt);
      update(dt);
      render();
    }
  }

  void GameEngine::stop() {
    _backend->getWindow().close();
  }

  void GameEngine::update(float dt) {
    if (!_quitAction.empty() && _actions.isJustPressed(_quitAction)) {
      stop();
      return;
    }
    if (!_muteAction.empty() && _actions.isJustPressed(_muteAction)) _audio.toggleMute();
    if (!_debugAction.empty() && _actions.isJustPressed(_debugAction)) _debug.overlayVisible = !_debug.overlayVisible;

    _sceneManager.update(dt);
    _audio.update();
  }

  void GameEngine::updateStats(float dt) {
    constexpr float window = 0.5f; 
    _statsElapsed += dt;
    ++_statsFrames;
    if (_statsElapsed >= window) {
      _stats.fps = static_cast<float>(_statsFrames) / _statsElapsed;
      _stats.frameTimeMs = 1000.0f * _statsElapsed / static_cast<float>(_statsFrames);
      _statsElapsed = 0.0f;
      _statsFrames = 0;
    }
  }

  void GameEngine::render() {
    IWindow& window = _backend->getWindow();
    window.clear(_clearColor);
    _sceneManager.render(window);
    window.display();
  }

  WindowConfig GameEngine::makeWindowConfig(const Config& config) {
    WindowConfig window;
    const int width = config.getInt("window", "width", static_cast<int>(window.width));
    const int height = config.getInt("window", "height", static_cast<int>(window.height));
    const int framerate = config.getInt("window", "framerate", static_cast<int>(window.framerateLimit));
    if (width <= 0 || height <= 0) throw RtypeError("[window] width and height must be positive");
    if (framerate < 0) throw RtypeError("[window] framerate must be >= 0 (0 = unlimited)");

    window.width = static_cast<unsigned int>(width);
    window.height = static_cast<unsigned int>(height);
    window.framerateLimit = static_cast<unsigned int>(framerate);
    window.title = config.getString("window", "title", window.title);
    window.vsync = config.getBool("window", "vsync", window.vsync);
    return window;
  }

  std::unique_ptr<IBackend> GameEngine::makeBackend(const std::string& name, const WindowConfig& window) {
    BackendFactory factory;
    backend::registerBuiltinBackends(factory);
    return factory.create(name, window);
  }
}
