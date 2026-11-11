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
    _font = config.getString(SECTION, "font");
    if (_font.empty()) return;

    _enabled = true;
    _template = config.getString(SECTION, "text", "FPS {fps}  {ms} ms  {sprites} sprites  {texts} texts");
    _refresh = config.getFloat(SECTION, "refresh", _refresh);
    _margin = config.getFloat(SECTION, "margin", 16.0f);
    _textSize = static_cast<unsigned int>(std::max(1, config.getInt(SECTION, "text_size", 12)));
    _lineHeight = static_cast<float>(_textSize) * std::max(1.0f, config.getFloat(SECTION, "line_spacing", 1.6f));
    _color = config.getColor(SECTION, "color", Color{120, 255, 120, 255});
    _outlineThickness = config.getFloat(SECTION, "outline_thickness", 2.0f);
    _zIndex = config.getInt(SECTION, "z_index", 1000);
  }

  Entity DebugOverlay::createLine() {
    const Entity entity = _registry.createEntity();
    _registry.addComponent(entity, TransformComponent{});
    _registry.addComponent(entity, ScreenAnchorComponent{{0.0f, 1.0f}, {_margin, -_margin}});

    TextComponent text;
    text.fontId = _font;
    text.characterSize = _textSize;
    text.color = _color;
    text.outlineThickness = _outlineThickness;
    text.origin = {0.0f, 1.0f};
    text.zIndex = _zIndex;
    text.visible = false;
    _registry.addComponent(entity, text);
    return entity;
  }

  void DebugOverlay::setVisible(std::size_t count) {
    for (std::size_t i = 0; i < _lines.size(); ++i)
      if (auto* text = _registry.getComponent<TextComponent>(_lines[i])) text->visible = i < count;
  }

  void DebugOverlay::update(float dt) {
    if (!_enabled) return;
    if (!_context.debug.overlayVisible) {
      setVisible(0);
      _elapsed = _refresh;
      return;
    }

    _elapsed += dt;
    if (_elapsed < _refresh && !_lines.empty()) {
      setVisible(_shown);
      return;
    }
    _elapsed = 0.0f;

    const FrameStats& stats = _context.stats;
    const auto ms = static_cast<long>(std::lround(stats.frameTimeMs * 10.0f));
    std::vector<std::string> rows{formatTemplate(_template, {
      {"fps", std::to_string(std::lround(stats.fps))},
      {"ms", std::to_string(ms / 10) + "." + std::to_string(ms % 10)},
      {"sprites", std::to_string(_renderer.getCachedSpriteCount())},
      {"texts", std::to_string(_renderer.getCachedTextCount())},
    })};
    for (const auto& [name, line] : _context.debug.lines)
      if (!line.empty()) rows.push_back(line);

    while (_lines.size() < rows.size()) _lines.push_back(createLine());
    for (std::size_t i = 0; i < rows.size(); ++i) {
      const float offsetY = -_margin - _lineHeight * static_cast<float>(rows.size() - 1 - i);
      if (auto* anchor = _registry.getComponent<ScreenAnchorComponent>(_lines[i])) anchor->offset = {_margin, offsetY};
      if (auto* text = _registry.getComponent<TextComponent>(_lines[i])) text->text = rows[i];
    }
    _shown = rows.size();
    setVisible(_shown);
  }
}
