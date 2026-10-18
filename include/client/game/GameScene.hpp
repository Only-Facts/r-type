#ifndef RTYPE_GAME_GAMESCENE_HPP
#define RTYPE_GAME_GAMESCENE_HPP

#include "engine/core/EngineContext.hpp"
#include "engine/scene/AScene.hpp"
#include "game/EntityCatalog.hpp"
#include "game/Hud.hpp"
#include "network/ClientNetworkManager.hpp"
#include "network/INetworkSocket.hpp"
#include <memory>
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

  private:
    void onEntitySpawned(engine::Entity entity, network::NetworkId netId, network::ext::EntityType type, bool isLocal);
    void onEntityDestroyed(network::NetworkId netId, network::ext::EntityType type);
    void onPlayerSpawned(engine::Entity entity, network::NetworkId netId, bool isLocal);
    void onPlayerDestroyed(network::NetworkId netId);
    void onHealthChanged(engine::Entity entity, network::NetworkId netId, network::ext::EntityType type,
                         int health, int maxHealth, int previous);
    void onLocalPlayerDestroyed();
    void flash(engine::Entity entity);
    void validateCatalog() const;
    void updateConnectionStatus(float dt);

    int playerNumber(network::NetworkId netId) const;
    std::string playerAssetFor(network::NetworkId netId) const;
    void playSound(const std::string& configKey);
    void playSoundId(const std::string& soundId);

    std::unique_ptr<network::INetworkSocket> _socket;
    client::ClientNetworkManager _network;
    Hud _hud;
    EntityCatalog _catalog;
    std::vector<network::ext::EntityType> _warnedTypes;
    std::unordered_set<network::NetworkId> _players;
    std::unordered_set<network::NetworkId> _deadPlayers;
    bool _localDead{false};

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
  };
}

#endif // !RTYPE_GAME_GAMESCENE_HPP
