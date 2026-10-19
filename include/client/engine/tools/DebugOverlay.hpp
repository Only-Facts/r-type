#ifndef RTYPE_ENGINE_DEBUGOVERLAY_HPP
#define RTYPE_ENGINE_DEBUGOVERLAY_HPP

#include "engine/core/EngineContext.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/systems/RenderSystem.hpp"
#include <string>

namespace rtype::engine {
  class DebugOverlay {
  public:
    DebugOverlay(Registry& registry, EngineContext& context, const RenderSystem& renderer)
      : _registry(registry), _context(context), _renderer(renderer) {}

    void init();
    void update(float dt);

  private:
    Registry& _registry;
    EngineContext& _context;
    const RenderSystem& _renderer;

    bool _enabled{false};
    Entity _entity{0};
    std::string _template;
    float _refresh{0.25f};
    float _elapsed{0.0f};
  };
}

#endif // !RTYPE_ENGINE_DEBUGOVERLAY_HPP
