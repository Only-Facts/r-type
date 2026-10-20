#include "game/SkinScene.hpp"
#include "engine/audio/AudioService.hpp"
#include "engine/config/Config.hpp"
#include "engine/core/TextTemplate.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/input/ActionMap.hpp"
#include "engine/platform/IWindow.hpp"
#include "engine/resources/ResourceManager.hpp"
#include "engine/scene/SceneManager.hpp"
#include "engine/systems/MovementSystem.hpp"
#include "engine/systems/ParallaxSystem.hpp"
#include "exception.hpp"
#include "game/StarfieldSpawner.hpp"
#include <algorithm>
#include <cstdint>

namespace rtype::game {
  namespace {
    constexpr const char* SECTION = "skin_select";

    unsigned int readSize(const engine::Config& config, const std::string& key, int fallback) {
      const int size = config.getInt(SECTION, key, fallback);
      if (size <= 0) throw RtypeError(std::string("[skin_select] ") + key + " must be > 0");
      return static_cast<unsigned int>(size);
    }
  }

  SkinScene::SkinScene(engine::EngineContext& context)
    : AScene(context),
      _font(context.config.getString(SECTION, "font", "font_hud")),
      _prevAction(context.config.getString(SECTION, "prev_action", "menu_prev")),
      _nextAction(context.config.getString(SECTION, "next_action", "menu_next")),
      _startAction(context.config.getString(SECTION, "start_action", "start")),
      _backAction(context.config.getString(SECTION, "back_action", "back")),
      _nextScene(context.config.getString(SECTION, "next_scene", "game")),
      _previousScene(context.config.getString(SECTION, "previous_scene", "title")),
      _carouselY(context.config.getFloat(SECTION, "carousel_y", 0.45f)),
      _spacing(context.config.getFloat(SECTION, "spacing", 0.16f)),
      _selectedScale(context.config.getFloat(SECTION, "selected_scale", 4.0f)),
      _otherScale(context.config.getFloat(SECTION, "other_scale", 2.0f)),
      _otherAlpha(static_cast<std::uint8_t>(std::clamp(context.config.getInt(SECTION, "other_alpha", 110), 0, 255))),
      _outlineThickness(context.config.getFloat(SECTION, "outline_thickness", 2.0f)) {
    _skins.load(context.config, "skins");
    if (_skins.empty()) throw RtypeError("[skins] list is empty: the ship selection needs at least one skin");
    for (std::size_t i = 0; i < _skins.count(); ++i) context.resources.get(_skins.at(i).asset, engine::AssetType::Texture);
    for (const char* key : {"change_sound", "confirm_sound", "back_sound"}) {
      const std::string sound = context.config.getString(SECTION, key);
      if (!sound.empty()) context.resources.get(sound, engine::AssetType::Sound);
    }
  }

  void SkinScene::onInit() {
    const engine::Config& cfg = _context.config;

    addSystem<engine::MovementSystem>();
    addSystem<engine::ParallaxSystem>(_context.window);
    StarfieldSpawner starfield(_registry, _factory);
    starfield.spawn(StarfieldSpawner::loadSettings(cfg, "starfield"), _context.window.getSize());

    const engine::Color white{255, 255, 255, 255};
    const engine::Color dim = cfg.getColor(SECTION, "dim_color", engine::Color{170, 170, 190, 255});

    addText(cfg.getString(SECTION, "title_text", "Choose your ship"), cfg.getFloat(SECTION, "title_y", 0.2f),
            readSize(cfg, "title_size", 32), cfg.getColor(SECTION, "title_color", white));

    for (std::size_t i = 0; i < _skins.count(); ++i) {
      const engine::Entity ship = _factory.createSprite(_skins.at(i).asset, {0.0f, 0.0f}, 10);
      _registry.addComponent(ship, engine::ScreenAnchorComponent{});
      _ships.push_back(ship);
    }

    _label = addText("", cfg.getFloat(SECTION, "label_y", 0.62f), readSize(cfg, "label_size", 20), white);

    addText(engine::formatTemplate(cfg.getString(SECTION, "hint_text", "{prev} - {next}   {start} to play   {back} to go back"),
                                   {{"prev", _context.actions.describe(_prevAction)},
                                    {"next", _context.actions.describe(_nextAction)},
                                    {"start", _context.actions.describe(_startAction)},
                                    {"back", _context.actions.describe(_backAction)}}),
            cfg.getFloat(SECTION, "hint_y", 0.85f), readSize(cfg, "hint_size", 12), dim);

    refresh();
  }

  void SkinScene::onUpdate(float) {
    const engine::ActionMap& actions = _context.actions;
    if (actions.isJustPressed(_prevAction)) change(-1);
    if (actions.isJustPressed(_nextAction)) change(1);

    if (actions.isJustPressed(_startAction)) {
      playConfigSound("confirm_sound");
      _context.scenes.changeScene(_nextScene);
    } else if (actions.isJustPressed(_backAction)) {
      playConfigSound("back_sound");
      _context.scenes.changeScene(_previousScene);
    }
  }

  void SkinScene::change(int step) {
    const auto count = static_cast<int>(_skins.count());
    if (count <= 1) return;
    const int current = static_cast<int>(_skins.selected(_context.session));
    _skins.select(_context.session, static_cast<std::size_t>(((current + step) % count + count) % count));
    refresh();
    playConfigSound("change_sound");
  }

  void SkinScene::refresh() {
    const std::size_t selected = _skins.selected(_context.session);
    const float center = static_cast<float>(_ships.size() - 1) / 2.0f;

    for (std::size_t i = 0; i < _ships.size(); ++i) {
      const bool isSelected = i == selected;
      const float scale = isSelected ? _selectedScale : _otherScale;
      const engine::AssetDescriptor& desc = _context.resources.get(_skins.at(i).asset, engine::AssetType::Texture);
      const float width = static_cast<float>(desc.rect ? desc.rect->width : 0) * desc.scale.x * scale;
      const float height = static_cast<float>(desc.rect ? desc.rect->height : 0) * desc.scale.y * scale;

      if (auto* anchor = _registry.getComponent<engine::ScreenAnchorComponent>(_ships[i])) {
        anchor->anchor = {0.5f + (static_cast<float>(i) - center) * _spacing, _carouselY};
        anchor->offset = {-width / 2.0f, -height / 2.0f};
      }
      if (auto* transform = _registry.getComponent<engine::TransformComponent>(_ships[i]))
        transform->scale = {scale, scale};
      if (auto* sprite = _registry.getComponent<engine::SpriteComponent>(_ships[i])) {
        engine::Color tint = desc.tint;
        if (!isSelected) tint.a = _otherAlpha;
        sprite->tint = tint;
      }
    }

    if (auto* label = _registry.getComponent<engine::TextComponent>(_label)) {
      const Skin& skin = _skins.at(selected);
      label->text = engine::formatTemplate(_context.config.getString(SECTION, "label_text", "< {skin} >"),
                                           {{"skin", skin.label}});
      label->color = skin.color;
    }
  }

  engine::Entity SkinScene::addText(const std::string& text, float anchorY, unsigned int size,
                                    const engine::Color& color) {
    const engine::Entity entity = _registry.createEntity();
    _registry.addComponent(entity, engine::TransformComponent{});

    engine::ScreenAnchorComponent anchor;
    anchor.anchor = {0.5f, anchorY};
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

  void SkinScene::playConfigSound(const std::string& key) {
    const std::string sound = _context.config.getString(SECTION, key);
    if (!sound.empty()) _context.audio.play(sound);
  }
}
