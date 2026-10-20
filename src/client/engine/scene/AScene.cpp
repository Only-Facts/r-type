#include "engine/scene/AScene.hpp"
#include "engine/platform/IWindow.hpp"
#include "engine/resources/ResourceManager.hpp"
#include "engine/systems/BlinkSystem.hpp"
#include "engine/systems/FlashSystem.hpp"
#include "engine/systems/FollowSystem.hpp"
#include "engine/systems/LifetimeSystem.hpp"
#include "engine/systems/ScreenAnchorSystem.hpp"

namespace rtype::engine {
  AScene::AScene(EngineContext& context)
    : _context(context),
      _factory(_registry, context.resources),
      _renderer(context.resources),
      _debugOverlay(_registry, context, _renderer) {
    _builtinSystems.add<ScreenAnchorSystem>(context.window);
    _builtinSystems.add<BlinkSystem>();
    _builtinSystems.add<FlashSystem>();
    _builtinSystems.add<FollowSystem>();
    _builtinSystems.add<LifetimeSystem>();
  }

  void AScene::init() {
    if (_initialized) return;
    _initialized = true;
    onInit();
    _debugOverlay.init();
  }

  void AScene::update(float dt) {
    onUpdate(dt);
    _systems.update(_registry, dt);
    _debugOverlay.update(dt);
    _builtinSystems.update(_registry, dt);
  }

  void AScene::render(IWindow& window) {
    _renderer.render(_registry, window);
  }

  void AScene::onUpdate(float) {}
}
