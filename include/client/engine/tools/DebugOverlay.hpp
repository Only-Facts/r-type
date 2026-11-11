#ifndef RTYPE_ENGINE_DEBUGOVERLAY_HPP
#define RTYPE_ENGINE_DEBUGOVERLAY_HPP

#include "engine/core/EngineContext.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/systems/RenderSystem.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace rtype::engine {
  class DebugOverlay {
  public:
    DebugOverlay(Registry& registry, EngineContext& context, const RenderSystem& renderer)
      : _registry(registry), _context(context), _renderer(renderer) {}

    void init();
    void update(float dt);

  private:
    Entity createLine();
    void setVisible(std::size_t count);

    Registry& _registry;
    EngineContext& _context;
    const RenderSystem& _renderer;

    bool _enabled{false};
    std::vector<Entity> _lines;
    std::size_t _shown{0};
    std::string _font;
    std::string _template;
    unsigned int _textSize{12};
    float _lineHeight{19.2f};
    float _margin{16.0f};
    float _outlineThickness{2.0f};
    int _zIndex{1000};
    Color _color{120, 255, 120, 255};
    float _refresh{0.25f};
    float _elapsed{0.0f};
  };
}

#endif // !RTYPE_ENGINE_DEBUGOVERLAY_HPP
