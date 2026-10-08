#ifndef GAMESCENE_HPP
#define GAMESCENE_HPP

#include "scene/IScene.hpp"
#include "ecs/Registry.hpp"
#include "ecs/Components.hpp"
#include "gfx/ITextureManager.hpp"
#include "input/IInputManager.hpp"
#include "network/SFMLUdpSocket.hpp"
#include "network/ClientNetworkManager.hpp"

namespace rtype::engine {
  class GameScene : public IScene {
  public:
    GameScene(ITextureManager& texMgr, IInputManager& inputMgr, std::string host = "127.0.0.1", unsigned short port = 4242)
      : _texMgr(texMgr), _inputMgr(inputMgr), _netMgr(_socket, _registry, _texMgr), _host(host), _port(port) {}

    ~GameScene() override = default;

    void init() override {
      _netMgr.connectToServer(_host, _port);
    }

    void update(float dt) override {
      bool up = _inputMgr.isKeyPressed(KeyCode::Up);
      bool down = _inputMgr.isKeyPressed(KeyCode::Down);
      bool left = _inputMgr.isKeyPressed(KeyCode::Left);
      bool right = _inputMgr.isKeyPressed(KeyCode::Right);

      _netMgr.sendInputs(up, down, left, right);
      _netMgr.update(dt);

      _registry.view<TransformComponent>([&](Entity e, TransformComponent& transform) {
        auto* spr = _registry.getComponent<SpriteComponent>(e);
        if (spr && spr->sprite) {
          spr->sprite->setPosition(transform.position);
          spr->sprite->setScale(transform.scale);
        }
      });
    }

    void render(IWindow& window) override {
      _registry.view<SpriteComponent>([&](Entity, SpriteComponent& spr) {
        if (spr.sprite)
          window.draw(*spr.sprite);
      });
    }

  private:
    Registry _registry;
    ITextureManager& _texMgr;
    IInputManager& _inputMgr;
    network::SFMLUdpSocket _socket;
    client::ClientNetworkManager _netMgr;

    std::string _host{"127.0.0.1"};
    unsigned short _port{4242};
  };
}

#endif // !GAMESCENE_HPP
