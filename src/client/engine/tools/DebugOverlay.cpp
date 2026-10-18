#include "engine/tools/DebugOverlay.hpp"
#include "engine/config/Config.hpp"
#include "engine/core/TextTemplate.hpp"
#include "engine/ecs/Components.hpp"
#include <algorithm>
#include <cmath>

namespace rtype::engine {
  namespace {
    constexpr const char* SECTION = "debug";
  }

  void DebugOverlay::init() {
    const Config& config = _context.config;
    const std::string font = config.getString(SECTION, "font");
    if (font.empty()) return; 

    _enabled = true;
    _template = config.getString(SECTION, "text", "FPS {fps}  {ms} ms  {sprites} sprites  {texts} texts");
    _refresh = config.getFloat(SECTION, "refresh", _refresh);
    const float margin = config.getFloat(SECTION, "margin", 16.0f);

    _entity = _registry.createEntity();
    _registry.addComponent(_entity, TransformComponent{});

    ScreenAnchorComponent anchor;
    anchor.anchor = {0.0f, 1.0f};
    anchor.offset = {margin, -margin};
    _registry.addComponent(_entity, anchor);

    TextComponent text;
    text.fontId = font;
    text.characterSize = static_cast<unsigned int>(std::max(1, config.getInt(SECTION, "text_size", 12)));
    text.color = config.getColor(SECTION, "color", Color{120, 255, 120, 255});
    text.outlineThickness = config.getFloat(SECTION, "outline_thickness", 2.0f);
    text.origin = {0.0f, 1.0f};
    text.zIndex = config.getInt(SECTION, "z_index", 1000);
    text.visible = _context.debug.overlayVisible;
    _registry.addComponent(_entity, text);
  }

  void DebugOverlay::update(float dt) {
    if (!_enabled) return;
    auto* text = _registry.getComponent<TextComponent>(_entity);
    if (!text) return;

    text->visible = _context.debug.overlayVisible;
    if (!text->visible) return;

    _elapsed += dt;
    if (_elapsed < _refresh && !text->text.empty()) return;
    _elapsed = 0.0f;

    const FrameStats& stats = _context.stats;
    const auto ms = static_cast<long>(std::lround(stats.frameTimeMs * 10.0f));
    text->text = formatTemplate(_template, {
      {"fps", std::to_string(std::lround(stats.fps))},
      {"ms", std::to_string(ms / 10) + "." + std::to_string(ms % 10)},
      {"sprites", std::to_string(_renderer.getCachedSpriteCount())},
      {"texts", std::to_string(_renderer.getCachedTextCount())},
    });
  }
}
