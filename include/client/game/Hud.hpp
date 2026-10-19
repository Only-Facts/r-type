#ifndef RTYPE_GAME_HUD_HPP
#define RTYPE_GAME_HUD_HPP

#include "engine/config/Config.hpp"
#include "engine/core/TextTemplate.hpp"
#include "engine/ecs/EntityFactory.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::game {
  class Hud {
  public:
    using Params = engine::TemplateParams;

    struct Settings {
      std::string fontId{"font_hud"};
      unsigned int textSize{16};
      float scale{1.0f};
      engine::Color color{255, 255, 255, 255};
      engine::Color outlineColor{0, 0, 0, 255};
      float outlineThickness{2.0f};
      float margin{24.0f};
      float notificationDuration{3.0f};
      float lineSpacing{1.8f};
      float notificationTop{2.0f};
      unsigned int bigTextSize{48};

      std::string barAsset{"ui_pixel"};
      engine::Vector2f barSize{240.0f, 12.0f};
      float barTop{1.2f};
      engine::Color barBackColor{40, 40, 60, 200};
      engine::Color healthHigh{90, 220, 120, 255};
      engine::Color healthLow{230, 70, 70, 255};
      float lowThreshold{0.3f};
      float augmentsTop{2.4f};
      int scoreDigits{7};
      engine::Color scoreColor{255, 207, 63, 255};
      engine::Vector2f bossBarSize{600.0f, 14.0f};
      engine::Color bossBarColor{230, 70, 70, 255};
      std::unordered_map<std::string, std::string> templates;
    };

    static Settings loadSettings(const engine::Config& config, const std::string& section);

    Hud(engine::Registry& registry, engine::EntityFactory& factory, Settings settings);

    void create();
    void setStatus(const std::string& templateKey, const Params& params, const engine::Color& color);
    void setPlayerCount(std::size_t count, std::size_t max);
    void setDifficulty(const std::string& label, const engine::Color& color);
    void notify(const std::string& templateKey, const Params& params);

    void setHealth(int current, int max);
    void setScore(std::uint32_t score);

    void showBoss(const std::string& label, const engine::Color& color);
    void setBossHealth(int current, int max);
    void hideBoss();

    void setAugment(std::uint8_t id, const std::string& label, const engine::Color& color, int level, float remaining);
    void removeAugment(std::uint8_t id);
    void clearAugments();
    bool hasAugment(std::uint8_t id) const { return _augments.find(id) != _augments.end(); }
    void update(float dt);

    void showCenterMessage(const std::string& templateKey, const std::string& hintKey, const Params& params,
                           const engine::Color& color);

  private:
    engine::Entity createText(const engine::Vector2f& anchor, const engine::Vector2f& offset,
                              const engine::Vector2f& origin, unsigned int size);
    engine::Entity createBar(const engine::Vector2f& anchor, const engine::Vector2f& offset, const engine::Vector2f& size,
                             const engine::Color& color, int zIndex);
    void setBarsVisible(std::initializer_list<engine::Entity> bars, bool visible);
    const std::string& getTemplate(const std::string& key) const;
    unsigned int scaled(unsigned int size) const;

    engine::Registry& _registry;
    engine::EntityFactory& _factory;
    Settings _settings;
    engine::Entity _status{0};
    engine::Entity _players{0};
    engine::Entity _difficulty{0};
    engine::Entity _score{0};
    engine::Entity _barBack{0};
    engine::Entity _barFill{0};
    engine::Entity _healthText{0};
    engine::Entity _center{0};
    engine::Entity _centerHint{0};
    engine::Entity _bossBack{0};
    engine::Entity _bossFill{0};
    engine::Entity _bossName{0};
    std::string _bossLabel;
    std::vector<engine::Entity> _notifications;

    struct AugmentSlot {
      engine::Entity entity{0};
      std::string label;
      int level{1};
      float remaining{0.0f};
      int shownSeconds{-1};
    };
    void layoutAugments();
    void refreshAugment(AugmentSlot& slot);
    std::map<std::uint8_t, AugmentSlot> _augments;
  };
}

#endif // !RTYPE_GAME_HUD_HPP
