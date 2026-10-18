#include "game/GameScene.hpp"
#include "engine/audio/AudioService.hpp"
#include "engine/config/Config.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/input/ActionMap.hpp"
#include "engine/platform/IBackend.hpp"
#include "engine/platform/IWindow.hpp"
#include "engine/resources/ResourceManager.hpp"
#include "engine/systems/AnimationSystem.hpp"
#include "engine/systems/InterpolationSystem.hpp"
#include "engine/systems/MovementSystem.hpp"
#include "engine/systems/ParallaxSystem.hpp"
#include "exception.hpp"
#include "game/StarfieldSpawner.hpp"
#include <algorithm>

namespace rtype::game {
  namespace {
    // Action names are the game's vocabulary; the keys behind them live in [input].
    constexpr const char* ACTION_UP = "move_up";
    constexpr const char* ACTION_DOWN = "move_down";
    constexpr const char* ACTION_LEFT = "move_left";
    constexpr const char* ACTION_RIGHT = "move_right";

    constexpr const char* SECTION_GAME = "game";
    constexpr const char* SECTION_NETWORK = "network";
    constexpr const char* SECTION_HUD = "hud";
    constexpr const char* SECTION_STARFIELD = "starfield";

    unsigned short readPort(const engine::Config& config) {
      const int port = config.getInt(SECTION_NETWORK, "port", 4242);
      if (port <= 0 || port > 65535) throw RtypeError("[network] port must be in [1, 65535]");
      return static_cast<unsigned short>(port);
    }
  }

  GameScene::GameScene(engine::EngineContext& context)
    : AScene(context),
      _socket(context.backend.createUdpSocket()),
      _network(*_socket, _registry),
      _hud(_registry, Hud::loadSettings(context.config, SECTION_HUD)),
      _host(context.config.getString(SECTION_NETWORK, "host", "127.0.0.1")),
      _port(readPort(context.config)),
      _connectTimeout(context.config.getFloat(SECTION_NETWORK, "connect_timeout", 5.0f)),
      _playerAssetPrefix(context.config.getString(SECTION_GAME, "player_asset_prefix", "player_")),
      _playerVariants(context.config.getInt(SECTION_GAME, "player_variants", 4)),
      _maxPlayers(context.config.getInt(SECTION_GAME, "max_players", 4)),
      _firstPlayerId(static_cast<network::NetworkId>(std::max(0, context.config.getInt(SECTION_GAME, "first_player_id", 1)))),
      _playerZ(context.config.getInt(SECTION_GAME, "player_z", 10)),
      _interpolationSpeed(context.config.getFloat(SECTION_NETWORK, "interpolation_speed", 15.0f)) {
    if (_playerVariants < 1) throw RtypeError("[game] player_variants must be >= 1");
    if (_maxPlayers < 1) throw RtypeError("[game] max_players must be >= 1");
  }

  GameScene::~GameScene() {
    _network.disconnect();
  }

  void GameScene::onInit() {
    // Engine systems, in execution order.
    addSystem<engine::AnimationSystem>();
    addSystem<engine::MovementSystem>();
    addSystem<engine::ParallaxSystem>(_context.window);
    addSystem<engine::InterpolationSystem>();

    StarfieldSpawner starfield(_registry, _factory);
    starfield.spawn(StarfieldSpawner::loadSettings(_context.config, SECTION_STARFIELD), _context.window.getSize());

    _hud.create();
    _hud.setStatus("connecting_text", {{"host", _host}, {"port", std::to_string(_port)}}, engine::Color{});
    _hud.setPlayerCount(0, static_cast<std::size_t>(_maxPlayers));

    const std::string music = _context.config.getString(SECTION_GAME, "music");
    if (!music.empty()) _context.audio.playMusic(music);

    _network.setOnEntitySpawned([this](engine::Entity entity, network::NetworkId netId, bool isLocal) {
      onPlayerSpawned(entity, netId, isLocal);
    });
    _network.setOnEntityDestroyed([this](engine::Entity, network::NetworkId netId) {
      onPlayerDestroyed(netId);
    });
    _network.connectToServer(_host, _port);
  }

  void GameScene::onUpdate(float dt) {
    const engine::ActionMap& actions = _context.actions;
    _network.sendInputs(actions.isActive(ACTION_UP), actions.isActive(ACTION_DOWN),
                        actions.isActive(ACTION_LEFT), actions.isActive(ACTION_RIGHT));
    _network.update(dt);
    updateConnectionStatus(dt);
  }

  void GameScene::updateConnectionStatus(float dt) {
    std::size_t players = 0;
    _registry.view<engine::NetworkIdComponent>([&players](engine::Entity, engine::NetworkIdComponent&) { ++players; });
    _hud.setPlayerCount(players, static_cast<std::size_t>(_maxPlayers));

    const bool connected = _network.isConnected();
    if (connected && !_wasConnected) {
      const network::NetworkId me = _network.getLocalNetId();
      // Status shown in the local player's color AND with its number (color is not the only cue).
      const engine::Color color = _context.resources.get(playerAssetFor(me)).tint;
      _hud.setStatus("connected_text", {{"player", std::to_string(playerNumber(me))}}, color);
    } else if (!connected) {
      _connectElapsed += dt;
      if (!_timeoutShown && _connectTimeout > 0.0f && _connectElapsed >= _connectTimeout) {
        _hud.setStatus("timeout_text", {{"host", _host}, {"port", std::to_string(_port)}}, engine::Color{255, 120, 120, 255});
        _timeoutShown = true;
      }
    }
    _wasConnected = connected;
  }

  void GameScene::onPlayerSpawned(engine::Entity entity, network::NetworkId netId, bool isLocal) {
    _factory.attachSprite(entity, playerAssetFor(netId), _playerZ);

    engine::InterpolationComponent interpolation;
    interpolation.speed = _interpolationSpeed;
    _registry.addComponent(entity, interpolation);

    playSound("join_sound");
    if (!isLocal) _hud.notify("joined_text", {{"player", std::to_string(playerNumber(netId))}});
  }

  void GameScene::onPlayerDestroyed(network::NetworkId netId) {
    playSound("leave_sound");
    _hud.notify("left_text", {{"player", std::to_string(playerNumber(netId))}});
  }

  int GameScene::playerNumber(network::NetworkId netId) const {
    // The first id the server hands out shows as "Player 1".
    const network::NetworkId index = netId >= _firstPlayerId ? netId - _firstPlayerId : 0;
    return static_cast<int>(index % static_cast<network::NetworkId>(_playerVariants)) + 1;
  }

  std::string GameScene::playerAssetFor(network::NetworkId netId) const {
    // Each player gets a distinct variant (subject: the 4 players must be identifiable).
    return _playerAssetPrefix + std::to_string(playerNumber(netId));
  }

  void GameScene::playSound(const std::string& configKey) {
    const std::string sound = _context.config.getString(SECTION_GAME, configKey);
    if (!sound.empty()) _context.audio.play(sound);
  }
}
