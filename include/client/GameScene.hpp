#ifndef GAMESCENE_HPP
#define GAMESCENE_HPP

#include "scene/IScene.hpp"
#include "ecs/Registry.hpp"
#include "ecs/Components.hpp"
#include "gfx/SFMLSprite.hpp"
#include "gfx/ITextureManager.hpp"
#include "input/IInputManager.hpp"

namespace rtype::engine {
  class GameScene : public IScene {
  public:
    GameScene(ITextureManager& texMgr, IInputManager& inputMgr)
      : _texMgr(texMgr), _inputMgr(inputMgr) {}

    ~GameScene() override = default;

    void init() override {
      _player = _registry.createEntity();

      auto tex = _texMgr.loadTexture("assets/player.png");
      auto sprite = std::make_shared<SFMLSprite>(tex);

      _registry.addComponent<TransformComponent>(_player, {{200.0f, 300.0f}, 0.0f, {2.0f, 2.0f}});
      _registry.addComponent<VelocityComponent>(_player, {{0.0f, 0.0f}});
      _registry.addComponent<SpriteComponent>(_player, {sprite});
    }

    void update(float dt) override {
      auto* vel = _registry.getComponent<VelocityComponent>(_player);
      if (vel) {
        vel->velocity = {0.0f, 0.0f};
        if (_inputMgr.isKeyPressed(KeyCode::Up)) vel->velocity.y -= 300.0f;
        if (_inputMgr.isKeyPressed(KeyCode::Down)) vel->velocity.y += 300.0f;
        if (_inputMgr.isKeyPressed(KeyCode::Left)) vel->velocity.x -= 300.0f;
        if (_inputMgr.isKeyPressed(KeyCode::Right)) vel->velocity.x += 300.0f;
      }

      _registry.view<TransformComponent>([&](Entity e, TransformComponent& transform) {
        auto* v = _registry.getComponent<VelocityComponent>(e);
        if (v) {
          transform.position.x += v->velocity.x * dt;
          transform.position.y += v->velocity.y * dt;
        }

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
    Entity _player{0};
  };
}

#endif // !GAMESCENE_HPP
