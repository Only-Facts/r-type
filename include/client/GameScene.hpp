#ifndef GAMESCENE_HPP
#define GAMESCENE_HPP

#include "scene/IScene.hpp"
#include "ecs/ISystem.hpp"
#include "ecs/Registry.hpp"
#include "ecs/Components.hpp"
#include "gfx/ITextFactory.hpp"
#include "gfx/ITextureManager.hpp"
#include "gfx/IBackground.hpp"
#include "gfx/Starfield.hpp"
#include "game/AnimationSystem.hpp"
#include "game/Hud.hpp"
#include "game/LifetimeSystem.hpp"
#include "game/MessageOverlay.hpp"
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
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rtype::engine {
  class GameScene : public IScene {
  public:
    GameScene(ITextureManager& texMgr,
              IInputManager& inputMgr,
              ITextFactory& textFactory,
              std::function<void()> onExit,
              std::string host = "127.0.0.1",
              unsigned short port = 4242)
      : _texMgr(texMgr),
        _inputMgr(inputMgr),
        _playerSpriteFactory(_texMgr),
        _missileSpriteFactory(_texMgr),
        _enemySpriteFactory(_texMgr),
        _explosionSpriteFactory(_texMgr),
        _netMgr(_socket, _registry, _playerSpriteFactory, _missileSpriteFactory, _enemySpriteFactory, _explosionSpriteFactory),
        _hud(textFactory, _playerSpriteFactory),
        _overlay(textFactory),
        _host(std::move(host)),
        _port(port),
        _onExit(std::move(onExit)) {
      _systems.push_back(std::make_unique<client::ProjectileSystem>());
      _systems.push_back(std::make_unique<client::AnimationSystem>());
      _systems.push_back(std::make_unique<client::LifetimeSystem>());
      _systems.push_back(std::make_unique<client::PlayerStatusSystem>());
      _systems.push_back(std::make_unique<client::ShipTiltSystem>());
    }

    ~GameScene() override {
      _netMgr.disconnect();
    }

    void init() override {
      _netMgr.connectToServer(_host, _port);
      setState(State::Connecting);
    }

    void update(float dt) override {
      _background->update(dt);
      _stateTimer += dt;

      if (_inputMgr.isKeyJustPressed(KeyCode::Escape)) {
        leave();
        return;
      }

      const bool canPlay = _state == State::Playing;
      _netMgr.sendInputs(
        canPlay && _inputMgr.isKeyPressed(KeyCode::Up),
        canPlay && _inputMgr.isKeyPressed(KeyCode::Down),
        canPlay && _inputMgr.isKeyPressed(KeyCode::Left),
        canPlay && _inputMgr.isKeyPressed(KeyCode::Right),
        canPlay && _inputMgr.isKeyPressed(KeyCode::Space));
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

      _hud.update(_registry);
      updateState();
    }

    void render(IWindow& window) override {
      _background->render(window);

      _registry.view<SpriteComponent>([&](Entity, SpriteComponent& spr) {
        if (spr.sprite)
          window.draw(*spr.sprite);
      });

      _hud.render(window);
      if (_state != State::Playing) _overlay.render(window);
    }

  private:
    enum class State { Connecting, Playing, GameOver, ConnectionFailed };

    static constexpr float CONNECT_TIMEOUT = 5.0f;
    static constexpr float GAME_OVER_DELAY = 1.2f;

    void updateState() {
      switch (_state) {
        case State::Connecting:
          if (_netMgr.isConnected()) setState(State::Playing);
          else if (_stateTimer >= CONNECT_TIMEOUT) {
            _netMgr.disconnect();
            setState(State::ConnectionFailed);
          }
          break;
        case State::Playing: {
          const PlayerComponent* localPlayer = findLocalPlayer();
          if (!localPlayer || localPlayer->lives > 0) _stateTimer = 0.0f;
          else if (_stateTimer >= GAME_OVER_DELAY) {
            _finalScore = localPlayer->score;
            setState(State::GameOver);
          }
          break;
        }
        case State::GameOver:
        case State::ConnectionFailed:
          if (_inputMgr.isKeyJustPressed(KeyCode::Enter)) leave();
          break;
      }
    }

    void setState(State state) {
      _state = state;
      _stateTimer = 0.0f;

      const std::string address = _host + ":" + std::to_string(_port);
      switch (state) {
        case State::Connecting:
          _overlay.setMessage("CONNECTING", address, "ESC  back to menu");
          break;
        case State::ConnectionFailed:
          _overlay.setMessage("CONNECTION FAILED", "No answer from " + address, "ENTER  back to menu");
          break;
        case State::GameOver:
          _overlay.setMessage("GAME OVER", "SCORE  " + client::Hud::formatScore(_finalScore), "ENTER  back to menu");
          break;
        case State::Playing:
          break;
      }
    }

    const PlayerComponent* findLocalPlayer() {
      const PlayerComponent* localPlayer = nullptr;
      _registry.view<PlayerComponent>([&](Entity entity, PlayerComponent& player) {
        auto* networkId = _registry.getComponent<NetworkIdComponent>(entity);
        if (networkId && networkId->isLocalPlayer) localPlayer = &player;
      });
      return localPlayer;
    }

    void leave() {
      if (_hasLeft) return;
      _hasLeft = true;
      _netMgr.disconnect();
      if (_onExit) _onExit();
    }

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
    client::Hud _hud;
    client::MessageOverlay _overlay;

    std::string _host{"127.0.0.1"};
    unsigned short _port{4242};
    std::function<void()> _onExit;

    State _state{State::Connecting};
    float _stateTimer{0.0f};
    std::uint32_t _finalScore{0};
    bool _hasLeft{false};
  };
}

#endif // !GAMESCENE_HPP
