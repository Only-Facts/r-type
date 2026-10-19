#include "game/Hud.hpp"
#include "engine/ecs/Components.hpp"
#include "exception.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace rtype::game {
  namespace {
    const std::unordered_map<std::string, std::string> DEFAULT_TEMPLATES{
      {"connecting_text", "Connecting to {host}:{port}..."},
      {"connected_text", "Connected - Player {player}"},
      {"timeout_text", "No answer from {host}:{port}"},
      {"players_text", "Players {count}/{max}"},
      {"joined_text", "Player {player} joined"},
      {"left_text", "Player {player} left the game"},
      {"health_text", "HP {hp}/{max}"},
      {"destroyed_text", "DESTROYED"},
      {"destroyed_hint_text", "Press {menu} for the menu, {keys} to quit"},
      {"player_destroyed_text", "Player {player} was destroyed"},
      {"difficulty_hud_text", "{difficulty}"},
      {"augment_text", "{label}  {time}s"},
      {"augment_stacked_text", "{label} x{level}  {time}s"},
      {"augment_permanent_text", "{label} x{level}"},
      {"score_text", "SCORE {score}"},
      {"boss_text", "{boss}  {hp}/{max}"},
      {"boss_warning_text", "WARNING - {boss} approaching"},
      {"boss_defeated_text", "{boss} destroyed"},
    };

    engine::Vector2f readPair(const engine::Config& config, const std::string& section, const std::string& key,
                              const engine::Vector2f& fallback) {
      if (!config.has(section, key)) return fallback;
      const auto values = config.getFloatList(section, key);
      if (values.size() != 2 || values[0] <= 0.0f || values[1] <= 0.0f)
        throw RtypeError("[" + section + "] " + key + " must be 'width height' (> 0)");
      return {values[0], values[1]};
    }
  }

  Hud::Settings Hud::loadSettings(const engine::Config& config, const std::string& section) {
    Settings s;
    s.fontId = config.getString(section, "font", s.fontId);
    const int size = config.getInt(section, "text_size", static_cast<int>(s.textSize));
    s.scale = config.getFloat(section, "scale", s.scale);
    s.color = config.getColor(section, "color", s.color);
    s.outlineColor = config.getColor(section, "outline_color", s.outlineColor);
    s.outlineThickness = config.getFloat(section, "outline_thickness", s.outlineThickness);
    s.margin = config.getFloat(section, "margin", s.margin);
    s.notificationDuration = config.getFloat(section, "notification_duration", s.notificationDuration);
    s.lineSpacing = config.getFloat(section, "line_spacing", s.lineSpacing);
    s.notificationTop = config.getFloat(section, "notification_top", s.notificationTop);
    s.bigTextSize = static_cast<unsigned int>(std::max(1, config.getInt(section, "big_text_size", static_cast<int>(s.bigTextSize))));
    s.barAsset = config.getString(section, "health_bar_asset", s.barAsset);
    s.barSize = readPair(config, section, "health_bar_size", s.barSize);
    s.barTop = config.getFloat(section, "health_bar_top", s.barTop);
    s.barBackColor = config.getColor(section, "health_bar_back_color", s.barBackColor);
    s.healthHigh = config.getColor(section, "health_color_high", s.healthHigh);
    s.healthLow = config.getColor(section, "health_color_low", s.healthLow);
    s.lowThreshold = config.getFloat(section, "health_low_threshold", s.lowThreshold);
    s.augmentsTop = config.getFloat(section, "augments_top", s.augmentsTop);
    s.scoreDigits = std::clamp(config.getInt(section, "score_digits", s.scoreDigits), 1, 10);
    s.scoreColor = config.getColor(section, "score_color", s.scoreColor);
    s.bossBarSize = readPair(config, section, "boss_bar_size", s.bossBarSize);
    s.bossBarColor = config.getColor(section, "boss_bar_color", s.bossBarColor);

    if (size <= 0) throw RtypeError("[" + section + "] text_size must be > 0");
    if (s.scale <= 0.0f) throw RtypeError("[" + section + "] scale must be > 0");
    s.textSize = static_cast<unsigned int>(size);

    for (const auto& [key, fallback] : DEFAULT_TEMPLATES)
      s.templates[key] = config.getString(section, key, fallback);
    return s;
  }

  Hud::Hud(engine::Registry& registry, engine::EntityFactory& factory, Settings settings)
    : _registry(registry), _factory(factory), _settings(std::move(settings)) {}

  void Hud::create() {
    const float m = _settings.margin;
    _status = createText({0.0f, 0.0f}, {m, m}, {0.0f, 0.0f}, scaled(_settings.textSize));
    _players = createText({1.0f, 0.0f}, {-m, m}, {1.0f, 0.0f}, scaled(_settings.textSize));

    const float lineHeight = static_cast<float>(scaled(_settings.textSize)) * _settings.lineSpacing;
    _difficulty = createText({1.0f, 0.0f}, {-m, m + lineHeight}, {1.0f, 0.0f}, scaled(_settings.textSize));
    _score = createText({1.0f, 0.0f}, {-m, m + lineHeight * 2.0f}, {1.0f, 0.0f}, scaled(_settings.textSize));
    if (auto* text = _registry.getComponent<engine::TextComponent>(_score)) text->color = _settings.scoreColor;
    const float barY = m + lineHeight * _settings.barTop;
    _barBack = createBar({0.0f, 0.0f}, {m, barY}, _settings.barSize, _settings.barBackColor, 90);
    _barFill = createBar({0.0f, 0.0f}, {m, barY}, _settings.barSize, _settings.healthHigh, 91);
    _healthText = createText({0.0f, 0.0f}, {m + _settings.barSize.x * _settings.scale + m / 2.0f, barY},
                             {0.0f, 0.0f}, scaled(_settings.textSize));

    _center = createText({0.5f, 0.42f}, {0.0f, 0.0f}, {0.5f, 0.5f}, scaled(_settings.bigTextSize));
    _centerHint = createText({0.5f, 0.42f}, {0.0f, static_cast<float>(scaled(_settings.bigTextSize))},
                             {0.5f, 0.0f}, scaled(_settings.textSize));

    // boss bar: centered at the top of the screen, name and health above it, notifications below
    const float bossWidth = _settings.bossBarSize.x * _settings.scale;
    const float bossBarY = m + lineHeight * 0.75f;
    _bossName = createText({0.5f, 0.0f}, {0.0f, m}, {0.5f, 0.0f}, scaled(_settings.textSize));
    _bossBack = createBar({0.5f, 0.0f}, {-bossWidth / 2.0f, bossBarY}, _settings.bossBarSize, _settings.barBackColor, 90);
    _bossFill = createBar({0.5f, 0.0f}, {-bossWidth / 2.0f, bossBarY}, _settings.bossBarSize, _settings.bossBarColor, 91);
  }

  engine::Entity Hud::createBar(const engine::Vector2f& anchor, const engine::Vector2f& offset,
                                const engine::Vector2f& size, const engine::Color& color, int zIndex) {
    const engine::Entity entity = _factory.createSprite(_settings.barAsset, {0.0f, 0.0f}, zIndex);

    engine::ScreenAnchorComponent screenAnchor;
    screenAnchor.anchor = anchor;
    screenAnchor.offset = offset;
    _registry.addComponent(entity, screenAnchor);

    if (auto* sprite = _registry.getComponent<engine::SpriteComponent>(entity)) {
      sprite->tint = color;
      sprite->visible = false;
    }
    if (auto* transform = _registry.getComponent<engine::TransformComponent>(entity))
      transform->scale = {size.x * _settings.scale, size.y * _settings.scale};
    return entity;
  }

  void Hud::setBarsVisible(std::initializer_list<engine::Entity> bars, bool visible) {
    for (engine::Entity bar : bars)
      if (auto* sprite = _registry.getComponent<engine::SpriteComponent>(bar)) sprite->visible = visible;
  }

  void Hud::setHealth(int current, int max) {
    const float ratio = max > 0 ? std::clamp(static_cast<float>(current) / static_cast<float>(max), 0.0f, 1.0f) : 0.0f;

    setBarsVisible({_barBack, _barFill}, true);

    if (auto* transform = _registry.getComponent<engine::TransformComponent>(_barFill))
      transform->scale.x = _settings.barSize.x * _settings.scale * ratio;
    if (auto* sprite = _registry.getComponent<engine::SpriteComponent>(_barFill))
      sprite->tint = ratio <= _settings.lowThreshold ? _settings.healthLow : _settings.healthHigh;
    if (auto* text = _registry.getComponent<engine::TextComponent>(_healthText))
      text->text = engine::formatTemplate(getTemplate("health_text"),
                                          {{"hp", std::to_string(current)}, {"max", std::to_string(max)}});
  }

  void Hud::setScore(std::uint32_t score) {
    std::string digits = std::to_string(score);
    if (digits.size() < static_cast<std::size_t>(_settings.scoreDigits))
      digits.insert(0, static_cast<std::size_t>(_settings.scoreDigits) - digits.size(), '0');
    if (auto* text = _registry.getComponent<engine::TextComponent>(_score))
      text->text = engine::formatTemplate(getTemplate("score_text"), {{"score", digits}});
  }

  void Hud::showBoss(const std::string& label, const engine::Color& color) {
    _bossLabel = label;
    setBarsVisible({_bossBack, _bossFill}, true);
    if (auto* text = _registry.getComponent<engine::TextComponent>(_bossName)) {
      text->text = label;
      text->color = color;
    }
  }

  void Hud::setBossHealth(int current, int max) {
    const float ratio = max > 0 ? std::clamp(static_cast<float>(current) / static_cast<float>(max), 0.0f, 1.0f) : 0.0f;
    if (auto* transform = _registry.getComponent<engine::TransformComponent>(_bossFill))
      transform->scale.x = _settings.bossBarSize.x * _settings.scale * ratio;
    if (auto* text = _registry.getComponent<engine::TextComponent>(_bossName))
      text->text = engine::formatTemplate(getTemplate("boss_text"), {{"boss", _bossLabel},
                                                                     {"hp", std::to_string(std::max(0, current))},
                                                                     {"max", std::to_string(max)}});
  }

  void Hud::hideBoss() {
    setBarsVisible({_bossBack, _bossFill}, false);
    if (auto* transform = _registry.getComponent<engine::TransformComponent>(_bossFill))
      transform->scale.x = _settings.bossBarSize.x * _settings.scale;
    if (auto* text = _registry.getComponent<engine::TextComponent>(_bossName)) text->text.clear();
    _bossLabel.clear();
  }

  void Hud::showCenterMessage(const std::string& templateKey, const std::string& hintKey, const Params& params,
                              const engine::Color& color) {
    if (auto* text = _registry.getComponent<engine::TextComponent>(_center)) {
      text->text = engine::formatTemplate(getTemplate(templateKey), params);
      text->color = color;
    }
    if (auto* hint = _registry.getComponent<engine::TextComponent>(_centerHint))
      hint->text = hintKey.empty() ? "" : engine::formatTemplate(getTemplate(hintKey), params);
  }

  void Hud::setStatus(const std::string& templateKey, const Params& params, const engine::Color& color) {
    if (auto* text = _registry.getComponent<engine::TextComponent>(_status)) {
      text->text = engine::formatTemplate(getTemplate(templateKey), params);
      text->color = color;
    }
  }

  void Hud::setDifficulty(const std::string& label, const engine::Color& color) {
    if (auto* text = _registry.getComponent<engine::TextComponent>(_difficulty)) {
      text->text = engine::formatTemplate(getTemplate("difficulty_hud_text"), {{"difficulty", label}});
      text->color = color;
    }
  }

  void Hud::setPlayerCount(std::size_t count, std::size_t max) {
    if (auto* text = _registry.getComponent<engine::TextComponent>(_players))
      text->text = engine::formatTemplate(getTemplate("players_text"), {{"count", std::to_string(count)}, {"max", std::to_string(max)}});
  }

  void Hud::notify(const std::string& templateKey, const Params& params) {
    _notifications.erase(std::remove_if(_notifications.begin(), _notifications.end(), [this](engine::Entity e) {
      return _registry.getComponent<engine::TextComponent>(e) == nullptr;
    }), _notifications.end());

    const unsigned int size = scaled(_settings.textSize);
    const float lineHeight = static_cast<float>(size) * _settings.lineSpacing;
    const float top = _settings.margin + lineHeight * _settings.notificationTop;

    for (std::size_t i = 0; i < _notifications.size(); ++i)
      if (auto* anchor = _registry.getComponent<engine::ScreenAnchorComponent>(_notifications[i]))
        anchor->offset.y = top + lineHeight * static_cast<float>(_notifications.size() - i);

    const engine::Entity entity = createText({0.5f, 0.0f}, {0.0f, top}, {0.5f, 0.0f}, size);
    if (auto* text = _registry.getComponent<engine::TextComponent>(entity))
      text->text = engine::formatTemplate(getTemplate(templateKey), params);

    engine::LifetimeComponent lifetime;
    lifetime.remaining = _settings.notificationDuration;
    _registry.addComponent(entity, lifetime);
    _notifications.push_back(entity);
  }

  engine::Entity Hud::createText(const engine::Vector2f& anchor, const engine::Vector2f& offset,
                                 const engine::Vector2f& origin, unsigned int size) {
    const engine::Entity entity = _registry.createEntity();
    _registry.addComponent(entity, engine::TransformComponent{});

    engine::ScreenAnchorComponent screenAnchor;
    screenAnchor.anchor = anchor;
    screenAnchor.offset = offset;
    _registry.addComponent(entity, screenAnchor);

    engine::TextComponent text;
    text.fontId = _settings.fontId;
    text.characterSize = size;
    text.color = _settings.color;
    text.outlineColor = _settings.outlineColor;
    text.outlineThickness = _settings.outlineThickness;
    text.origin = origin;
    _registry.addComponent(entity, text);
    return entity;
  }

  const std::string& Hud::getTemplate(const std::string& key) const {
    auto it = _settings.templates.find(key);
    if (it == _settings.templates.end()) throw RtypeError("Unknown HUD text template: " + key);
    return it->second;
  }

  unsigned int Hud::scaled(unsigned int size) const {
    return static_cast<unsigned int>(std::lround(static_cast<float>(size) * _settings.scale));
  }

  void Hud::setAugment(std::uint8_t id, const std::string& label, const engine::Color& color, int level,
                       float remaining) {
    auto it = _augments.find(id);
    if (it == _augments.end()) {
      AugmentSlot slot;
      slot.entity = createText({0.0f, 0.0f}, {_settings.margin, 0.0f}, {0.0f, 0.0f}, scaled(_settings.textSize));
      it = _augments.emplace(id, slot).first;
      layoutAugments();
    }
    AugmentSlot& slot = it->second;
    slot.label = label;
    slot.level = level;
    slot.remaining = remaining;
    slot.shownSeconds = -1;
    if (auto* text = _registry.getComponent<engine::TextComponent>(slot.entity)) text->color = color;
    refreshAugment(slot);
  }

  void Hud::removeAugment(std::uint8_t id) {
    auto it = _augments.find(id);
    if (it == _augments.end()) return;
    _registry.destroyEntity(it->second.entity);
    _augments.erase(it);
    layoutAugments();
  }

  void Hud::clearAugments() {
    for (auto& [id, slot] : _augments) _registry.destroyEntity(slot.entity);
    _augments.clear();
  }

  void Hud::update(float dt) {
    for (auto& [id, slot] : _augments) {
      if (slot.remaining < 0.0f) continue;
      slot.remaining = std::max(0.0f, slot.remaining - dt);
      refreshAugment(slot);
    }
  }

  void Hud::layoutAugments() {
    const float lineHeight = static_cast<float>(scaled(_settings.textSize)) * _settings.lineSpacing;
    float index = 0.0f;
    for (auto& [id, slot] : _augments) {
      if (auto* anchor = _registry.getComponent<engine::ScreenAnchorComponent>(slot.entity))
        anchor->offset.y = _settings.margin + lineHeight * (_settings.augmentsTop + index);
      index += 1.0f;
    }
  }

  void Hud::refreshAugment(AugmentSlot& slot) {
    const bool permanent = slot.remaining < 0.0f;
    const int seconds = permanent ? -1 : static_cast<int>(std::ceil(slot.remaining));
    if (seconds == slot.shownSeconds && !permanent) return;
    slot.shownSeconds = seconds;
    if (auto* text = _registry.getComponent<engine::TextComponent>(slot.entity)) {
      const std::string& templ = getTemplate(permanent ? "augment_permanent_text"
                                                       : slot.level > 1 ? "augment_stacked_text" : "augment_text");
      text->text = engine::formatTemplate(templ, {{"label", slot.label},
                                                  {"level", std::to_string(slot.level)},
                                                  {"time", std::to_string(seconds)}});
    }
  }
}
