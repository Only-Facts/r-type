#include "game/GameScene.hpp"
#include "engine/audio/AudioService.hpp"
#include "engine/config/Config.hpp"
#include "engine/core/TextTemplate.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/input/ActionMap.hpp"
#include "engine/input/KeyCodes.hpp"
#include "engine/platform/IBackend.hpp"
#include "engine/platform/IWindow.hpp"
#include "engine/resources/ResourceManager.hpp"
#include "engine/scene/SceneManager.hpp"
#include "engine/systems/AnimationSystem.hpp"
#include "engine/systems/InterpolationSystem.hpp"
#include "engine/systems/MovementSystem.hpp"
#include "engine/systems/ParallaxSystem.hpp"
#include "exception.hpp"
#include "game/StarfieldSpawner.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>

namespace rtype::game {
  namespace {
    constexpr const char* ACTION_UP = "move_up";
    constexpr const char* ACTION_DOWN = "move_down";
    constexpr const char* ACTION_LEFT = "move_left";
    constexpr const char* ACTION_RIGHT = "move_right";
    constexpr const char* ACTION_SHOOT = "shoot";

    constexpr const char* SECTION_GAME = "game";
    constexpr const char* SECTION_NETWORK = "network";
    constexpr const char* SECTION_HUD = "hud";
    constexpr const char* SECTION_STARFIELD = "starfield";
    constexpr const char* SECTION_ENTITY_TYPES = "entity_types";
    constexpr const char* SECTION_AUGMENTS = "augments";
    constexpr const char* SECTION_FLOATING = "floating_text";
    constexpr const char* SESSION_NET_ID = "player_net_id";

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
      _hud(_registry, _factory, Hud::loadSettings(context.config, SECTION_HUD)),
      _host(context.config.getString(SECTION_NETWORK, "host", "127.0.0.1")),
      _port(readPort(context.config)),
      _connectTimeout(context.config.getFloat(SECTION_NETWORK, "connect_timeout", 5.0f)),
      _playerAssetPrefix(context.config.getString(SECTION_GAME, "player_asset_prefix", "player_")),
      _playerVariants(context.config.getInt(SECTION_GAME, "player_variants", 4)),
      _maxPlayers(context.config.getInt(SECTION_GAME, "max_players", 4)),
      _firstPlayerId(static_cast<network::NetworkId>(std::max(0, context.config.getInt(SECTION_GAME, "first_player_id", 1)))),
      _playerZ(context.config.getInt(SECTION_GAME, "player_z", 10)),
      _interpolationSpeed(context.config.getFloat(SECTION_NETWORK, "interpolation_speed", 15.0f)),
      _hitFlashColor(context.config.getColor(SECTION_GAME, "hit_flash_color", engine::Color{255, 80, 80, 255})),
      _hitFlashDuration(context.config.getFloat(SECTION_GAME, "hit_flash_duration", 0.15f)),
      _menuAction(context.config.getString(SECTION_GAME, "menu_action", "menu")),
      _menuScene(context.config.getString(SECTION_GAME, "menu_scene", "title")),
      _screenSize{static_cast<float>(context.config.getInt("window", "width", 1920)),
                  static_cast<float>(context.config.getInt("window", "height", 1080))} {
    if (_playerVariants < 1) throw RtypeError("[game] player_variants must be >= 1");
    if (_maxPlayers < 1) throw RtypeError("[game] max_players must be >= 1");
    _catalog.load(context.config, SECTION_ENTITY_TYPES);
    _augments.load(context.config, SECTION_AUGMENTS);
    _skins.load(context.config, "skins");
    const engine::Config& cfg = context.config;
    _floating.font = cfg.getString(SECTION_FLOATING, "font", cfg.getString(SECTION_HUD, "font", "font_hud"));
    _floating.size = static_cast<unsigned int>(std::max(1, cfg.getInt(SECTION_FLOATING, "text_size", 14)));
    _floating.riseSpeed = cfg.getFloat(SECTION_FLOATING, "rise_speed", _floating.riseSpeed);
    _floating.duration = cfg.getFloat(SECTION_FLOATING, "duration", _floating.duration);
    _floating.fadePortion = cfg.getFloat(SECTION_FLOATING, "fade_portion", _floating.fadePortion);
    _floating.offsetX = cfg.getFloat(SECTION_FLOATING, "offset_x", _floating.offsetX);
    _floating.offsetY = cfg.getFloat(SECTION_FLOATING, "offset_y", _floating.offsetY);
    _floating.outline = cfg.getFloat(SECTION_FLOATING, "outline_thickness", _floating.outline);
    _floating.healColor = cfg.getColor(SECTION_FLOATING, "heal_color", _floating.healColor);
    _floating.zIndex = cfg.getInt(SECTION_FLOATING, "z", _floating.zIndex);
    if (_floating.duration <= 0.0f) throw RtypeError("[floating_text] duration must be > 0");
    validateCatalog();
  }

  void GameScene::validateCatalog() const {
    for (int id = 0; id < 256; ++id) {
      const EntityProfile* profile = _catalog.find(static_cast<network::ext::EntityType>(id));
      if (!profile) continue;
      _context.resources.get(profile->asset, engine::AssetType::Texture);
      if (!profile->spawnSound.empty()) _context.resources.get(profile->spawnSound, engine::AssetType::Sound);
      if (!profile->destroySound.empty()) _context.resources.get(profile->destroySound, engine::AssetType::Sound);
      if (!profile->hitSound.empty()) _context.resources.get(profile->hitSound, engine::AssetType::Sound);
    }
    for (const char* key : {"join_sound", "leave_sound", "player_hit_sound", "player_death_sound", "game_over_sound"}) {
      const std::string sound = _context.config.getString(SECTION_GAME, key);
      if (!sound.empty()) _context.resources.get(sound, engine::AssetType::Sound);
    }
    _augments.forEach([this](std::uint8_t, const AugmentProfile& profile) {
      if (!profile.gainSound.empty()) _context.resources.get(profile.gainSound, engine::AssetType::Sound);
      if (!profile.endSound.empty()) _context.resources.get(profile.endSound, engine::AssetType::Sound);
    });
    for (const char* key : {"music", "boss_music"}) {
      const std::string music = _context.config.getString(SECTION_GAME, key);
      if (!music.empty()) _context.resources.get(music, engine::AssetType::Music);
    }
    _context.resources.get(_floating.font, engine::AssetType::Font);
    for (std::size_t i = 0; i < _skins.count(); ++i) _context.resources.get(_skins.at(i).asset, engine::AssetType::Texture);
    const std::string heal = _context.config.getString(SECTION_GAME, "player_heal_sound");
    if (!heal.empty()) _context.resources.get(heal, engine::AssetType::Sound);
  }

  GameScene::~GameScene() {
    _network.disconnect();
  }

  void GameScene::exit() {
    _network.disconnect();
    if (_bossMusicPlaying) playStageMusic();
  }

  void GameScene::playStageMusic() {
    _bossMusicPlaying = false;
    const std::string music = _context.config.getString(SECTION_GAME, "music");
    if (music.empty()) _context.audio.stopMusic();
    else _context.audio.playMusic(music);
  }

  void GameScene::onInit() {
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

    _network.setOnEntitySpawned(
      [this](engine::Entity entity, network::NetworkId netId, network::ext::EntityType type, bool isLocal) {
        onEntitySpawned(entity, netId, type, isLocal);
      });
    _network.setOnEntityDestroyed([this](engine::Entity, network::NetworkId netId, network::ext::EntityType type) {
      onEntityDestroyed(netId, type);
    });
    _network.setOnHealthChanged([this](engine::Entity entity, network::NetworkId netId, network::ext::EntityType type,
                                       int health, int maxHealth, int previous) {
      onHealthChanged(entity, netId, type, health, maxHealth, previous);
    });
    _network.setOnAugmentChanged([this](engine::Entity entity, network::NetworkId netId, network::ext::EntityType,
                                        network::ext::AugmentId augment, int level, float remaining) {
      onAugmentChanged(entity, netId, augment, level, remaining);
    });
    _network.setOnScoreChanged([this](network::NetworkId netId, std::uint32_t score) {
      if (netId == _network.getLocalNetId()) _hud.setScore(score);
    });
    _difficulty.load(_context.config, "difficulty");
    std::optional<std::uint8_t> skin;
    if (!_skins.empty()) skin = static_cast<std::uint8_t>(_skins.selected(_context.session));
    // ask for the id of our previous game, so we keep the same player number after a game over
    std::optional<network::NetworkId> previousNetId;
    if (const int id = _context.session.getInt(SESSION_NET_ID, -1); id > 0)
      previousNetId = static_cast<network::NetworkId>(id);
    _network.connectToServer(_host, _port,
                             static_cast<std::uint8_t>(_difficulty.selected(_context.session)), skin, previousNetId);
  }

  void GameScene::onUpdate(float dt) {
    if (_localDead && _context.actions.isJustPressed(_menuAction)) {
      _context.scenes.changeScene(_menuScene);
      return;
    }
    sendInputs();
    _network.update(dt);
    keepLocalShipOnScreen();
    _hud.update(dt);
    updateConnectionStatus(dt);
  }

  void GameScene::sendInputs() {
    if (_localDead) {
      _network.sendInputs(false, false, false, false, false);
      return;
    }
    const engine::ActionMap& actions = _context.actions;
    bool up = actions.isActive(ACTION_UP);
    bool down = actions.isActive(ACTION_DOWN);
    bool left = actions.isActive(ACTION_LEFT);
    bool right = actions.isActive(ACTION_RIGHT);
    // the server does not bound movement: drop any input pushing the ship past a screen edge
    if (const auto* transform = _localShip ? _registry.getComponent<engine::TransformComponent>(*_localShip) : nullptr) {
      const engine::Vector2f& pos = transform->targetPosition;
      if (pos.x <= 0.0f) left = false;
      if (pos.y <= 0.0f) up = false;
      if (pos.x + _localShipSize.x >= _screenSize.x) right = false;
      if (pos.y + _localShipSize.y >= _screenSize.y) down = false;
    }
    _network.sendInputs(up, down, left, right, actions.isActive(ACTION_SHOOT));
  }

  void GameScene::keepLocalShipOnScreen() {
    if (!_localShip) return;
    auto* transform = _registry.getComponent<engine::TransformComponent>(*_localShip);
    if (!transform) return;
    const float maxX = std::max(0.0f, _screenSize.x - _localShipSize.x);
    const float maxY = std::max(0.0f, _screenSize.y - _localShipSize.y);
    for (engine::Vector2f* pos : {&transform->position, &transform->targetPosition}) {
      pos->x = std::clamp(pos->x, 0.0f, maxX);
      pos->y = std::clamp(pos->y, 0.0f, maxY);
    }
  }

  void GameScene::updateConnectionStatus(float dt) {
    _hud.setPlayerCount(_players.size(), static_cast<std::size_t>(_maxPlayers));

    const bool connected = _network.isConnected();
    if (connected && !_wasConnected) {
      const network::NetworkId me = _network.getLocalNetId();
      _context.session.setInt(SESSION_NET_ID, static_cast<int>(me));
      const engine::Color color = playerColor(me);
      _hud.setStatus("connected_text", {{"player", std::to_string(playerNumber(me))}}, color);
      if (const auto difficulty = _network.getServerDifficulty(); difficulty && *difficulty < _difficulty.count()) {
        const DifficultyLevel& level = _difficulty.at(*difficulty);
        _hud.setDifficulty(level.label, level.color);
      }
    } else if (!connected) {
      _connectElapsed += dt;
      if (!_timeoutShown && _connectTimeout > 0.0f && _connectElapsed >= _connectTimeout) {
        _hud.setStatus("timeout_text", {{"host", _host}, {"port", std::to_string(_port)}}, engine::Color{255, 120, 120, 255});
        _timeoutShown = true;
      }
    }
    _wasConnected = connected;
  }

  void GameScene::onEntitySpawned(engine::Entity entity, network::NetworkId netId, network::ext::EntityType type,
                                  bool isLocal) {
    if (type == network::ext::ENTITY_PLAYER) {
      onPlayerSpawned(entity, netId, isLocal);
      return;
    }

    const EntityProfile* profile = _catalog.find(type);
    if (!profile) {
      if (std::find(_warnedTypes.begin(), _warnedTypes.end(), type) == _warnedTypes.end()) {
        _warnedTypes.push_back(type);
        std::cerr << "[warning] entity type " << static_cast<int>(type) << " has no [entity_types] entry\n";
      }
      return;
    }

    _factory.attachSprite(entity, profile->asset, profile->zIndex);
    if (auto* transform = _registry.getComponent<engine::TransformComponent>(entity))
      transform->scale = {profile->scale, profile->scale};
    if (profile->interpolationSpeed > 0.0f) {
      engine::InterpolationComponent interpolation;
      interpolation.speed = profile->interpolationSpeed;
      _registry.addComponent(entity, interpolation);
    }
    playSoundId(profile->spawnSound);
    if (profile->boss) onBossSpawned(netId, *profile);
  }

  void GameScene::onBossSpawned(network::NetworkId netId, const EntityProfile& profile) {
    _bossNetId = netId;
    _bossLabel = profile.label;
    _hud.showBoss(profile.label, profile.color);
    _hud.notify("boss_warning_text", {{"boss", profile.label}});
    const std::string music = _context.config.getString(SECTION_GAME, "boss_music");
    if (!music.empty()) {
      _context.audio.playMusic(music);
      _bossMusicPlaying = true;
    }
  }

  void GameScene::onHealthChanged(engine::Entity entity, network::NetworkId netId, network::ext::EntityType type,
                                  int health, int maxHealth, int previous) {
    const bool isPlayer = type == network::ext::ENTITY_PLAYER;
    const bool isLocal = isPlayer && netId == _network.getLocalNetId();

    if (isLocal) _hud.setHealth(health, maxHealth);
    if (_bossNetId && netId == *_bossNetId) _hud.setBossHealth(health, maxHealth);

    if (health > previous && previous > 0) {
      spawnFloatingText(entity,
                        engine::formatTemplate(_context.config.getString(SECTION_FLOATING, "heal_text", "+{amount} HP"),
                                               {{"amount", std::to_string(health - previous)}}),
                        _floating.healColor);
      if (isPlayer) playSound("player_heal_sound");
    }

    if (health < previous) {
      flash(entity);
      if (isPlayer) playSound("player_hit_sound");
      else if (const EntityProfile* profile = _catalog.find(type)) playSoundId(profile->hitSound);
    }

    if (health <= 0 && previous > 0 && isPlayer) {
      _deadPlayers.insert(netId);
      if (isLocal) {
        onLocalPlayerDestroyed();
      } else {
        playSound("player_death_sound");
        _hud.notify("player_destroyed_text", {{"player", std::to_string(playerNumber(netId))}});
      }
    }
  }

  void GameScene::onLocalPlayerDestroyed() {
    _localDead = true;
    _hud.clearAugments();
    playSound("player_death_sound");
    playSound("game_over_sound");

    const std::string keys = _context.actions.describe(_context.config.getString("engine", "quit_action", "quit"));
    _hud.showCenterMessage("destroyed_text", "destroyed_hint_text",
                           {{"keys", keys}, {"menu", _context.actions.describe(_menuAction)}},
                           _context.config.getColor(SECTION_HUD, "destroyed_color", engine::Color{255, 90, 90, 255}));
  }

  void GameScene::onAugmentChanged(engine::Entity entity, network::NetworkId netId, network::ext::AugmentId augment,
                                   int level, float remaining) {
    const AugmentProfile* profile = _augments.find(augment);
    if (!profile) return;
    const bool isLocal = netId == _network.getLocalNetId();

    if (remaining == 0.0f) {
      if (isLocal && _hud.hasAugment(augment)) {
        _hud.removeAugment(augment);
        playSoundId(profile->endSound);
      }
      return;
    }

    const bool gained = !isLocal || !_hud.hasAugment(augment);
    if (isLocal) _hud.setAugment(augment, profile->label, profile->color, level, remaining);
    if (gained) {
      spawnFloatingText(entity,
                        engine::formatTemplate(_context.config.getString(SECTION_FLOATING, "augment_text", "{label}!"),
                                               {{"label", profile->label}, {"level", std::to_string(level)}}),
                        profile->color);
      if (isLocal) playSoundId(profile->gainSound);
    }
  }

  void GameScene::spawnFloatingText(engine::Entity anchor, const std::string& text, const engine::Color& color) {
    const auto* transform = _registry.getComponent<engine::TransformComponent>(anchor);
    if (!transform || text.empty()) return;

    const engine::Entity entity = _registry.createEntity();
    engine::TransformComponent position;
    position.position = {transform->position.x + _floating.offsetX, transform->position.y + _floating.offsetY};
    _registry.addComponent(entity, position);

    engine::VelocityComponent velocity;
    velocity.velocity = {0.0f, -_floating.riseSpeed};
    _registry.addComponent(entity, velocity);

    engine::TextComponent label;
    label.fontId = _floating.font;
    label.text = text;
    label.characterSize = _floating.size;
    label.color = color;
    label.outlineThickness = _floating.outline;
    label.origin = {0.5f, 1.0f};
    label.zIndex = _floating.zIndex;
    _registry.addComponent(entity, label);

    engine::LifetimeComponent lifetime;
    lifetime.remaining = _floating.duration;
    lifetime.duration = _floating.duration;
    lifetime.fadeOut = true;
    lifetime.fadePortion = _floating.fadePortion;
    _registry.addComponent(entity, lifetime);
  }

  void GameScene::flash(engine::Entity entity) {
    if (_hitFlashDuration <= 0.0f) return;
    auto* flashComponent = _registry.getComponent<engine::FlashComponent>(entity);
    if (!flashComponent) {
      engine::FlashComponent component;
      component.color = _hitFlashColor;
      component.duration = _hitFlashDuration;
      flashComponent = &_registry.addComponent(entity, component);
    }
    flashComponent->remaining = flashComponent->duration;
  }

  void GameScene::onEntityDestroyed(network::NetworkId netId, network::ext::EntityType type) {
    if (type == network::ext::ENTITY_PLAYER) {
      onPlayerDestroyed(netId);
      return;
    }
    if (const EntityProfile* profile = _catalog.find(type)) playSoundId(profile->destroySound);
    if (_bossNetId && netId == *_bossNetId) {
      _bossNetId.reset();
      _hud.hideBoss();
      _hud.notify("boss_defeated_text", {{"boss", _bossLabel}});
      if (_bossMusicPlaying) playStageMusic();
    }
  }

  void GameScene::onPlayerSpawned(engine::Entity entity, network::NetworkId netId, bool isLocal) {
    _players.insert(netId);
    const std::string asset = playerAssetFor(netId);
    _factory.attachSprite(entity, asset, _playerZ);
    if (isLocal) {
      const engine::IntRect rect = _context.resources.createSprite(asset)->getTextureRect();
      const engine::Vector2f scale = _context.resources.get(asset, engine::AssetType::Texture).scale;
      const auto* transform = _registry.getComponent<engine::TransformComponent>(entity);
      const engine::Vector2f entityScale = transform ? transform->scale : engine::Vector2f{1.0f, 1.0f};
      _localShip = entity;
      _localShipSize = {static_cast<float>(rect.width) * scale.x * entityScale.x,
                        static_cast<float>(rect.height) * scale.y * entityScale.y};
    }
    addPlayerTag(entity, netId);

    engine::InterpolationComponent interpolation;
    interpolation.speed = _interpolationSpeed;
    _registry.addComponent(entity, interpolation);

    playSound("join_sound");
    if (!isLocal) _hud.notify("joined_text", {{"player", std::to_string(playerNumber(netId))}});
  }

  void GameScene::onPlayerDestroyed(network::NetworkId netId) {
    _players.erase(netId);
    if (netId == _network.getLocalNetId()) _localShip.reset();
    if (_deadPlayers.erase(netId) > 0) return;
    playSound("leave_sound");
    _hud.notify("left_text", {{"player", std::to_string(playerNumber(netId))}});
  }

  int GameScene::playerNumber(network::NetworkId netId) const {
    const network::NetworkId index = netId >= _firstPlayerId ? netId - _firstPlayerId : 0;
    return static_cast<int>(index % static_cast<network::NetworkId>(_playerVariants)) + 1;
  }

  std::string GameScene::playerAssetFor(network::NetworkId netId) const {
    if (!_skins.empty()) {
      if (netId == _network.getLocalNetId()) return _skins.at(_skins.selected(_context.session)).asset;
      if (const auto skin = _network.getPlayerSkin(netId))
        if (const auto index = _skins.find(*skin)) return _skins.at(*index).asset;
    }
    return _playerAssetPrefix + std::to_string(playerNumber(netId));
  }

  engine::Color GameScene::playerColor(network::NetworkId netId) const {
    return _context.resources.get(_playerAssetPrefix + std::to_string(playerNumber(netId))).tint;
  }

  void GameScene::addPlayerTag(engine::Entity ship, network::NetworkId netId) {
    const engine::Config& cfg = _context.config;
    const std::string text = engine::formatTemplate(cfg.getString(SECTION_GAME, "player_tag_text", "P{player}"),
                                                    {{"player", std::to_string(playerNumber(netId))}});
    if (text.empty()) return;

    const engine::Entity tag = _registry.createEntity();
    _registry.addComponent(tag, engine::TransformComponent{});

    engine::FollowComponent follow;
    follow.target = ship;
    const auto offset = cfg.getFloatList(SECTION_GAME, "player_tag_offset");
    follow.offset = offset.size() == 2 ? engine::Vector2f{offset[0], offset[1]} : engine::Vector2f{17.0f, -2.0f};
    _registry.addComponent(tag, follow);

    engine::TextComponent label;
    label.fontId = cfg.getString(SECTION_HUD, "font", "font_hud");
    label.text = text;
    label.characterSize = static_cast<unsigned int>(std::max(1, cfg.getInt(SECTION_GAME, "player_tag_size", 10)));
    label.color = playerColor(netId);
    label.outlineThickness = cfg.getFloat(SECTION_GAME, "player_tag_outline", 2.0f);
    label.origin = {0.5f, 1.0f};
    label.zIndex = _playerZ + 1;
    _registry.addComponent(tag, label);
  }

  void GameScene::playSound(const std::string& configKey) {
    playSoundId(_context.config.getString(SECTION_GAME, configKey));
  }

  void GameScene::playSoundId(const std::string& soundId) {
    if (!soundId.empty()) _context.audio.play(soundId);
  }
}
