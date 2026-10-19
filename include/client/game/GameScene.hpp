#ifndef RTYPE_GAME_GAMESCENE_HPP
#define RTYPE_GAME_GAMESCENE_HPP

#include "engine/core/EngineContext.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/scene/AScene.hpp"
#include "game/AugmentCatalog.hpp"
#include "game/Difficulty.hpp"
#include "game/EntityCatalog.hpp"
#include "game/Hud.hpp"
#include "game/SkinCatalog.hpp"
#include "network/ClientNetworkManager.hpp"
#include "network/INetworkSocket.hpp"
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace rtype::game {
  class GameScene : public engine::AScene {
  public:
    explicit GameScene(engine::EngineContext& context);
    ~GameScene() override;

  protected:
    void onInit() override;
    void onUpdate(float dt) override;

  public:
    void exit() override;

  private:
    void onEntitySpawned(engine::Entity entity, network::NetworkId netId, network::ext::EntityType type, bool isLocal);
    void onEntityDestroyed(network::NetworkId netId, network::ext::EntityType type);
    void onPlayerSpawned(engine::Entity entity, network::NetworkId netId, bool isLocal);
    void onPlayerDestroyed(network::NetworkId netId);
    void onHealthChanged(engine::Entity entity, network::NetworkId netId, network::ext::EntityType type,
                         int health, int maxHealth, int previous);
    void onLocalPlayerDestroyed();
    void onBossSpawned(network::NetworkId netId, const EntityProfile& profile);
    void playStageMusic();
    void flash(engine::Entity entity);
    void onAugmentChanged(engine::Entity entity, network::NetworkId netId, network::ext::AugmentId augment, int level,
                          float remaining);
    void spawnFloatingText(engine::Entity anchor, const std::string& text, const engine::Color& color);
    void validateCatalog() const;
    void updateConnectionStatus(float dt);
    void sendInputs();
    void keepLocalShipOnScreen();

    int playerNumber(network::NetworkId netId) const;
    std::string playerAssetFor(network::NetworkId netId) const;
    engine::Color playerColor(network::NetworkId netId) const;
    void addPlayerTag(engine::Entity ship, network::NetworkId netId);
    void playSound(const std::string& configKey);
    void playSoundId(const std::string& soundId);

    std::unique_ptr<network::INetworkSocket> _socket;
    client::ClientNetworkManager _network;
    Hud _hud;
    EntityCatalog _catalog;
    Difficulty _difficulty;
    AugmentCatalog _augments;
    SkinCatalog _skins;

    struct FloatingTextStyle {
      std::string font;
      unsigned int size{14};
      float riseSpeed{40.0f};
      float duration{1.0f};
      float fadePortion{0.5f};
      float offsetX{16.0f};
      float offsetY{-12.0f};
      float outline{2.0f};
      int zIndex{200};
      engine::Color healColor{120, 255, 140, 255};
    };
    FloatingTextStyle _floating;
    std::vector<network::ext::EntityType> _warnedTypes;
    std::unordered_set<network::NetworkId> _players;
    std::unordered_set<network::NetworkId> _deadPlayers;
    bool _localDead{false};
    std::optional<engine::Entity> _localShip;
    std::optional<network::NetworkId> _bossNetId;
    std::string _bossLabel;
    bool _bossMusicPlaying{false};
    engine::Vector2f _localShipSize{0.0f, 0.0f};

    std::string _host;
    unsigned short _port;
    float _connectTimeout;
    float _connectElapsed{0.0f};
    bool _wasConnected{false};
    bool _timeoutShown{false};

    std::string _playerAssetPrefix;
    int _playerVariants;
    int _maxPlayers;
    network::NetworkId _firstPlayerId;
    int _playerZ;
    float _interpolationSpeed;
    engine::Color _hitFlashColor;
    float _hitFlashDuration;
    std::string _menuAction;
    std::string _menuScene;
    engine::Vector2f _screenSize;
  };
}

#endif // !RTYPE_GAME_GAMESCENE_HPP
