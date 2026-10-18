#include "engine/scene/AScene.hpp"
#include "engine/resources/ResourceManager.hpp"

namespace rtype::engine {
  AScene::AScene(EngineContext& context)
    : _context(context),
      _factory(_registry, context.resources),
      _renderer(context.resources) {}

  void AScene::init() {
    if (_initialized) return;
    _initialized = true;
    onInit();
  }

  void AScene::update(float dt) {
    onUpdate(dt);
    _systems.update(_registry, dt);
  }

  void AScene::render(IWindow& window) {
    _renderer.render(_registry, window);
  }

  void AScene::onUpdate(float) {}
}
