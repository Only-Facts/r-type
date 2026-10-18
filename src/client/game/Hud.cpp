#include "game/Hud.hpp"
#include "ecs/Components.hpp"
#include <algorithm>
#include <set>

namespace rtype::client {
  namespace {
    constexpr unsigned int CHARACTER_SIZE = 28;
    constexpr int SCORE_DIGITS = 7;
    constexpr std::uint8_t MAX_DISPLAYED_LIVES = 5;

    constexpr float MARGIN = 20.0f;
    constexpr float PADDING = 12.0f;
    constexpr float ROW_HEIGHT = 44.0f;
    constexpr float TEXT_OFFSET_Y = 4.0f;
    constexpr float SCORE_COLUMN_X = 64.0f;
    constexpr float LIVES_COLUMN_X = 210.0f;

    constexpr float LIFE_ICON_SCALE = 1.2f;
    constexpr float LIFE_ICON_WIDTH = 33.0f * LIFE_ICON_SCALE;
    constexpr float LIFE_ICON_HEIGHT = 17.0f * LIFE_ICON_SCALE;
    constexpr float LIFE_ICON_SPACING = 6.0f;

    constexpr float PANEL_WIDTH = LIVES_COLUMN_X + MAX_DISPLAYED_LIVES * (LIFE_ICON_WIDTH + LIFE_ICON_SPACING) + PADDING;

    constexpr engine::Color PANEL_COLOR{0, 0, 0, 120};
    constexpr engine::Color LOCAL_SCORE_COLOR{255, 255, 255, 255};
    constexpr engine::Color REMOTE_SCORE_COLOR{185, 185, 185, 255};
    constexpr std::uint8_t DEAD_PLAYER_ALPHA = 110;
  }

  Hud::Hud(engine::ITextFactory& textFactory, IPlayerSpriteFactory& playerSpriteFactory)
    : _textFactory(textFactory), _playerSpriteFactory(playerSpriteFactory) {}

  void Hud::update(engine::Registry& registry) {
    std::set<network::NetworkId> presentPlayers;

    registry.view<engine::PlayerComponent>([&](engine::Entity entity, engine::PlayerComponent& player) {
      auto* networkId = registry.getComponent<engine::NetworkIdComponent>(entity);
      if (!networkId) return;

      const network::NetworkId playerId = networkId->netId;
      presentPlayers.insert(playerId);

      PlayerLine& line = getOrCreateLine(playerId);
      line.lives = std::min(player.lives, MAX_DISPLAYED_LIVES);

      const std::uint8_t alpha = player.lives > 0 ? 255 : DEAD_PLAYER_ALPHA;
      const engine::Color scoreColor = networkId->isLocalPlayer ? LOCAL_SCORE_COLOR : REMOTE_SCORE_COLOR;

      line.score->setString(formatScore(player.score));
      line.score->setColor(withAlpha(scoreColor, alpha));
      line.label->setColor(withAlpha(_playerSpriteFactory.getPlayerColor(playerId), alpha));
    });

    std::erase_if(_lines, [&](const auto& entry) { return !presentPlayers.contains(entry.first); });
    layoutLines();
  }

  void Hud::render(engine::IWindow& window) const {
    if (_lines.empty()) return;

    const float panelHeight = static_cast<float>(_lines.size()) * ROW_HEIGHT + PADDING;
    window.drawRectangle({MARGIN, MARGIN}, {PANEL_WIDTH, panelHeight}, PANEL_COLOR);

    float rowTop = MARGIN + PADDING / 2.0f;
    for (const auto& [playerId, line] : _lines) {
      window.draw(*line.label);
      window.draw(*line.score);

      const float iconY = rowTop + (ROW_HEIGHT - LIFE_ICON_HEIGHT) / 2.0f;
      for (std::uint8_t i = 0; i < line.lives; ++i) {
        const float iconX = MARGIN + LIVES_COLUMN_X + i * (LIFE_ICON_WIDTH + LIFE_ICON_SPACING);
        line.lifeIcon->setPosition({iconX, iconY});
        window.draw(*line.lifeIcon);
      }
      rowTop += ROW_HEIGHT;
    }
  }

  Hud::PlayerLine& Hud::getOrCreateLine(network::NetworkId playerId) {
    auto it = _lines.find(playerId);
    if (it != _lines.end()) return it->second;

    PlayerLine line;
    line.label = _textFactory.createText(CHARACTER_SIZE);
    line.label->setString("P" + std::to_string(playerId));
    line.score = _textFactory.createText(CHARACTER_SIZE);
    line.lifeIcon = _playerSpriteFactory.createSprite(playerId);
    line.lifeIcon->setScale({LIFE_ICON_SCALE, LIFE_ICON_SCALE});

    return _lines.emplace(playerId, std::move(line)).first->second;
  }

  void Hud::layoutLines() {
    float rowTop = MARGIN + PADDING / 2.0f;
    for (auto& [playerId, line] : _lines) {
      line.label->setPosition({MARGIN + PADDING, rowTop + TEXT_OFFSET_Y});
      line.score->setPosition({MARGIN + SCORE_COLUMN_X, rowTop + TEXT_OFFSET_Y});
      rowTop += ROW_HEIGHT;
    }
  }

  std::string Hud::formatScore(std::uint32_t score) {
    std::string digits = std::to_string(score);
    if (digits.size() < SCORE_DIGITS) digits.insert(0, SCORE_DIGITS - digits.size(), '0');
    return digits;
  }

  engine::Color Hud::withAlpha(engine::Color color, std::uint8_t alpha) {
    color.a = alpha;
    return color;
  }
}
