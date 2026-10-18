#include "game/TitleScene.hpp"
#include "engine/audio/AudioService.hpp"
#include "engine/config/Config.hpp"
#include "engine/core/TextTemplate.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/input/ActionMap.hpp"
#include "engine/input/KeyCodes.hpp"
#include "engine/platform/IWindow.hpp"
#include "engine/scene/SceneManager.hpp"
#include "engine/systems/MovementSystem.hpp"
#include "engine/systems/ParallaxSystem.hpp"
#include "exception.hpp"
#include "game/StarfieldSpawner.hpp"
#include <algorithm>
#include <cctype>

namespace rtype::game {
  namespace {
    constexpr const char* SECTION = "title";

    unsigned int readSize(const engine::Config& config, const std::string& key, int fallback) {
      const int size = config.getInt(SECTION, key, fallback);
      if (size <= 0) throw RtypeError(std::string("[title] ") + key + " must be > 0");
      return static_cast<unsigned int>(size);
    }
  }

  TitleScene::TitleScene(engine::EngineContext& context)
    : AScene(context),
      _font(context.config.getString(SECTION, "font", "font_hud")),
      _startAction(context.config.getString(SECTION, "start_action", "start")),
      _nextScene(context.config.getString(SECTION, "next_scene", "game")),
      _outlineThickness(context.config.getFloat(SECTION, "outline_thickness", 2.0f)) {}

  void TitleScene::onInit() {
    const engine::Config& cfg = _context.config;

    addSystem<engine::MovementSystem>();
    addSystem<engine::ParallaxSystem>(_context.window);
    StarfieldSpawner starfield(_registry, _factory);
    starfield.spawn(StarfieldSpawner::loadSettings(cfg, "starfield"), _context.window.getSize());

    const engine::Color white{255, 255, 255, 255};
    const engine::Color accent = cfg.getColor(SECTION, "accent_color", engine::Color{120, 200, 255, 255});
    const engine::Color dim = cfg.getColor(SECTION, "dim_color", engine::Color{170, 170, 190, 255});

    const float titleY = cfg.getFloat(SECTION, "title_y", 0.22f);
    const float promptY = cfg.getFloat(SECTION, "prompt_y", 0.45f);
    const float controlsY = cfg.getFloat(SECTION, "controls_y", 0.58f);
    const float subtitleSpacing = cfg.getFloat(SECTION, "subtitle_spacing", 1.3f);
    const float lineSpacing = cfg.getFloat(SECTION, "line_spacing", 2.0f);

    const unsigned int titleSize = readSize(cfg, "title_size", 72);
    addText(cfg.getString(SECTION, "title_text", "R-TYPE"), titleY, 0.0f, titleSize,
            cfg.getColor(SECTION, "title_color", white));
    addText(cfg.getString(SECTION, "subtitle_text", ""), titleY, static_cast<float>(titleSize) * subtitleSpacing,
            readSize(cfg, "subtitle_size", 14), dim);

    const engine::Entity prompt = addText(
      engine::formatTemplate(cfg.getString(SECTION, "prompt_text", "Press {keys} to start"), {{"keys", keysFor(_startAction)}}),
      promptY, 0.0f, readSize(cfg, "prompt_size", 20), accent);
    const float blink = cfg.getFloat(SECTION, "prompt_blink_period", 1.2f);
    if (blink > 0.0f) {
      engine::BlinkComponent blinkComponent;
      blinkComponent.period = blink;
      _registry.addComponent(prompt, blinkComponent);
    }

    const unsigned int controlSize = readSize(cfg, "controls_size", 14);
    const float lineHeight = static_cast<float>(controlSize) * lineSpacing;
    float y = 0.0f;
    addText(cfg.getString(SECTION, "controls_title", "Controls"), controlsY, y, controlSize, accent);
    for (const auto& action : cfg.getList(SECTION, "controls")) {
      if (!_context.actions.has(action)) continue;
      y += lineHeight;
      const std::string label = cfg.getString(SECTION, "label_" + action, action);
      addText(engine::formatTemplate(cfg.getString(SECTION, "control_line", "{label}  {keys}"),
                                     {{"label", label}, {"keys", keysFor(action)}}),
              controlsY, y, controlSize, white);
    }

    addText(engine::formatTemplate(cfg.getString(SECTION, "server_text", "Server {host}:{port}"),
                                   {{"host", cfg.getString("network", "host", "127.0.0.1")},
                                    {"port", cfg.getString("network", "port", "4242")}}),
            cfg.getFloat(SECTION, "server_y", 0.94f), 0.0f, controlSize, dim);

    const std::string music = cfg.getString(SECTION, "music");
    if (!music.empty()) _context.audio.playMusic(music);
  }

  void TitleScene::onUpdate(float) {
    if (_context.actions.isJustPressed(_startAction)) {
      const std::string sound = _context.config.getString(SECTION, "start_sound");
      if (!sound.empty()) _context.audio.play(sound);
      _context.scenes.changeScene(_nextScene);
    }
  }

  engine::Entity TitleScene::addText(const std::string& text, float anchorY, float offsetY, unsigned int size,
                                     const engine::Color& color) {
    const engine::Entity entity = _registry.createEntity();
    _registry.addComponent(entity, engine::TransformComponent{});

    engine::ScreenAnchorComponent anchor;
    anchor.anchor = {0.5f, anchorY};
    anchor.offset = {0.0f, offsetY};
    _registry.addComponent(entity, anchor);

    engine::TextComponent component;
    component.fontId = _font;
    component.text = text;
    component.characterSize = size;
    component.color = color;
    component.outlineThickness = _outlineThickness;
    component.origin = {0.5f, 0.0f};
    _registry.addComponent(entity, component);
    return entity;
  }

  std::string TitleScene::keysFor(const std::string& action) const {
    std::string result;
    for (engine::KeyCode key : _context.actions.getBindings(action)) {
      std::string name(engine::keyCodeToString(key));
      std::transform(name.begin(), name.end(), name.begin(),
                     [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
      result += (result.empty() ? "" : " / ") + name;
    }
    return result.empty() ? "-" : result;
  }
}
