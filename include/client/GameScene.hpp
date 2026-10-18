#ifndef GAMESCENE_HPP
#define GAMESCENE_HPP

#include "scene/IScene.hpp"
#include "ecs/ISystem.hpp"
#include "ecs/Registry.hpp"
#include "ecs/Components.hpp"
#include "gfx/ITextureManager.hpp"
#include "gfx/IBackground.hpp"
#include "gfx/Starfield.hpp"
#include "game/AnimationSystem.hpp"
#include "game/LifetimeSystem.hpp"
#include "game/PlayerStatusSystem.hpp"
#include "game/ProjectileSystem.hpp"
#include "game/SFMLEnemySpriteFactory.hpp"
#include "game/SFMLExplosionSpriteFactory.hpp"
#include "game/ShipTiltSystem.hpp"
#include "game/SFMLMissileSpriteFactory.hpp"
#include "game/SFMLPlayerSpriteFactory.hpp"
#include "input/IInputManager.hpp"
#include "network/SFMLUdpSocket.hpp"
#include "network/ClientNetworkManager.hpp"
#include <memory>
#include <vector>

namespace rtype::engine {
  class GameScene : public IScene {
  public:
    GameScene(ITextureManager& texMgr, IInputManager& inputMgr, std::string host = "127.0.0.1", unsigned short port = 4242)
      : _texMgr(texMgr),
        _inputMgr(inputMgr),
        _playerSpriteFactory(_texMgr),
        _missileSpriteFactory(_texMgr),
        _enemySpriteFactory(_texMgr),
        _explosionSpriteFactory(_texMgr),
        _netMgr(_socket, _registry, _playerSpriteFactory, _missileSpriteFactory, _enemySpriteFactory, _explosionSpriteFactory),
        _host(host),
        _port(port) {
      _systems.push_back(std::make_unique<client::ProjectileSystem>());
      _systems.push_back(std::make_unique<client::AnimationSystem>());
      _systems.push_back(std::make_unique<client::LifetimeSystem>());
      _systems.push_back(std::make_unique<client::PlayerStatusSystem>());
      _systems.push_back(std::make_unique<client::ShipTiltSystem>());
    }

    ~GameScene() override = default;

    void init() override {
      _netMgr.connectToServer(_host, _port);
    }

    void update(float dt) override {
      _background->update(dt);

      bool up = _inputMgr.isKeyPressed(KeyCode::Up);
      bool down = _inputMgr.isKeyPressed(KeyCode::Down);
      bool left = _inputMgr.isKeyPressed(KeyCode::Left);
      bool right = _inputMgr.isKeyPressed(KeyCode::Right);
      bool shoot = _inputMgr.isKeyPressed(KeyCode::Space);

      _netMgr.sendInputs(up, down, left, right, shoot);
      _netMgr.update(dt);

      for (auto& system : _systems)
        system->update(_registry, dt);

      _registry.view<TransformComponent>([&](Entity e, TransformComponent& transform) {
        auto* spr = _registry.getComponent<SpriteComponent>(e);
        if (spr && spr->sprite) {
          spr->sprite->setPosition(transform.position);
          spr->sprite->setScale(transform.scale);
        }
      });
    }

    void render(IWindow& window) override {
      _background->render(window);

      _registry.view<SpriteComponent>([&](Entity, SpriteComponent& spr) {
        if (spr.sprite)
          window.draw(*spr.sprite);
      });
    }

  private:
    Registry _registry;
    std::unique_ptr<IBackground> _background{std::make_unique<Starfield>()};
    std::vector<std::unique_ptr<ISystem>> _systems;
    ITextureManager& _texMgr;
    IInputManager& _inputMgr;
    client::SFMLPlayerSpriteFactory _playerSpriteFactory;
    client::SFMLMissileSpriteFactory _missileSpriteFactory;
    client::SFMLEnemySpriteFactory _enemySpriteFactory;
    client::SFMLExplosionSpriteFactory _explosionSpriteFactory;
    network::SFMLUdpSocket _socket;
    client::ClientNetworkManager _netMgr;

    std::string _host{"127.0.0.1"};
    unsigned short _port{4242};
  };
}

#endif // !GAMESCENE_HPP
