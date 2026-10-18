#include "game/Hud.hpp"
#include "engine/ecs/Components.hpp"
#include "exception.hpp"
#include <algorithm>
#include <cmath>

namespace rtype::game {
  namespace {
    // Template keys looked up in the [hud] section, with an English fallback.
    const std::unordered_map<std::string, std::string> DEFAULT_TEMPLATES{
      {"connecting_text", "Connecting to {host}:{port}..."},
      {"connected_text", "Connected - Player {player}"},
      {"timeout_text", "No answer from {host}:{port}"},
      {"players_text", "Players {count}/{max}"},
      {"joined_text", "Player {player} joined"},
      {"left_text", "Player {player} left the game"},
    };
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

    if (size <= 0) throw RtypeError("[" + section + "] text_size must be > 0");
    if (s.scale <= 0.0f) throw RtypeError("[" + section + "] scale must be > 0");
    s.textSize = static_cast<unsigned int>(size);

    for (const auto& [key, fallback] : DEFAULT_TEMPLATES)
      s.templates[key] = config.getString(section, key, fallback);
    return s;
  }

  std::string Hud::format(const std::string& templ, const Params& params) {
    std::string result = templ;
    for (const auto& [key, value] : params) {
      const std::string token = "{" + key + "}";
      for (auto pos = result.find(token); pos != std::string::npos; pos = result.find(token, pos + value.size()))
        result.replace(pos, token.size(), value);
    }
    return result;
  }

  Hud::Hud(engine::Registry& registry, Settings settings)
    : _registry(registry), _settings(std::move(settings)) {}

  void Hud::create() {
    const float m = _settings.margin;
    _status = createText({0.0f, 0.0f}, {m, m}, {0.0f, 0.0f}, scaled(_settings.textSize));
    _players = createText({1.0f, 0.0f}, {-m, m}, {1.0f, 0.0f}, scaled(_settings.textSize));
  }

  void Hud::setStatus(const std::string& templateKey, const Params& params, const engine::Color& color) {
    if (auto* text = _registry.getComponent<engine::TextComponent>(_status)) {
      text->text = format(getTemplate(templateKey), params);
      text->color = color;
    }
  }

  void Hud::setPlayerCount(std::size_t count, std::size_t max) {
    if (auto* text = _registry.getComponent<engine::TextComponent>(_players))
      text->text = format(getTemplate("players_text"), {{"count", std::to_string(count)}, {"max", std::to_string(max)}});
  }

  void Hud::notify(const std::string& templateKey, const Params& params) {
    // Forget notifications already destroyed by the LifetimeSystem.
    _notifications.erase(std::remove_if(_notifications.begin(), _notifications.end(), [this](engine::Entity e) {
      return _registry.getComponent<engine::TextComponent>(e) == nullptr;
    }), _notifications.end());

    const unsigned int size = scaled(_settings.textSize);
    const float lineHeight = static_cast<float>(size) * 1.8f;
    const float top = _settings.margin + lineHeight * 2.0f;

    // Newest on top, older ones pushed down.
    for (std::size_t i = 0; i < _notifications.size(); ++i)
      if (auto* anchor = _registry.getComponent<engine::ScreenAnchorComponent>(_notifications[i]))
        anchor->offset.y = top + lineHeight * static_cast<float>(_notifications.size() - i);

    const engine::Entity entity = createText({0.5f, 0.0f}, {0.0f, top}, {0.5f, 0.0f}, size);
    if (auto* text = _registry.getComponent<engine::TextComponent>(entity))
      text->text = format(getTemplate(templateKey), params);

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
}
