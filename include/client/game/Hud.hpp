#ifndef RTYPE_GAME_HUD_HPP
#define RTYPE_GAME_HUD_HPP

#include "engine/config/Config.hpp"
#include "engine/core/TextTemplate.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::game {
  /// In-game HUD built from ECS entities (Text + ScreenAnchor [+ Lifetime]).
  /// Every text is a template from the [hud] config section, with {placeholders},
  /// so wording and language can change without recompiling.
  class Hud {
  public:
    using Params = engine::TemplateParams;

    struct Settings {
      std::string fontId{"font_hud"};
      unsigned int textSize{16};
      float scale{1.0f};  ///< Accessibility: enlarges every HUD text.
      engine::Color color{255, 255, 255, 255};
      engine::Color outlineColor{0, 0, 0, 255};
      float outlineThickness{2.0f};
      float margin{24.0f};
      float notificationDuration{3.0f};
      std::unordered_map<std::string, std::string> templates;
    };

    static Settings loadSettings(const engine::Config& config, const std::string& section);

    Hud(engine::Registry& registry, Settings settings);

    void create();
    void setStatus(const std::string& templateKey, const Params& params, const engine::Color& color);
    void setPlayerCount(std::size_t count, std::size_t max);
    void notify(const std::string& templateKey, const Params& params);

  private:
    engine::Entity createText(const engine::Vector2f& anchor, const engine::Vector2f& offset,
                              const engine::Vector2f& origin, unsigned int size);
    const std::string& getTemplate(const std::string& key) const;
    unsigned int scaled(unsigned int size) const;

    engine::Registry& _registry;
    Settings _settings;
    engine::Entity _status{0};
    engine::Entity _players{0};
    std::vector<engine::Entity> _notifications;
  };
}

#endif // !RTYPE_GAME_HUD_HPP
