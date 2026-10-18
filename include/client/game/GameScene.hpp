#ifndef RTYPE_GAME_GAMESCENE_HPP
#define RTYPE_GAME_GAMESCENE_HPP

#include "engine/core/EngineContext.hpp"
#include "engine/scene/AScene.hpp"
#include "game/Hud.hpp"
#include "network/ClientNetworkManager.hpp"
#include "network/INetworkSocket.hpp"
#include <memory>
#include <string>

namespace rtype::game {
  /// The R-Type in-game scene. It only talks to engine interfaces: it asks for
  /// assets and sounds by id, reads actions by name, gets its socket from the
  /// backend, and reads its tuning from the [game], [network], [hud] and
  /// [starfield] config sections.
  class GameScene : public engine::AScene {
  public:
    explicit GameScene(engine::EngineContext& context);
    ~GameScene() override;

  protected:
    void onInit() override;
    void onUpdate(float dt) override;

  private:
    void onPlayerSpawned(engine::Entity entity, network::NetworkId netId, bool isLocal);
    void onPlayerDestroyed(network::NetworkId netId);
    void updateConnectionStatus(float dt);

    int playerNumber(network::NetworkId netId) const;
    std::string playerAssetFor(network::NetworkId netId) const;
    void playSound(const std::string& configKey);

    std::unique_ptr<network::INetworkSocket> _socket;
    client::ClientNetworkManager _network;
    Hud _hud;

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
  };
}

#endif // !RTYPE_GAME_GAMESCENE_HPP
