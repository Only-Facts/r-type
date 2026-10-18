#ifndef HUD_HPP
#define HUD_HPP

#include "IWindow.hpp"
#include "ecs/Registry.hpp"
#include "game/IPlayerSpriteFactory.hpp"
#include "gfx/ITextFactory.hpp"
#include "network/Protocol.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace rtype::client {
  class Hud {
  public:
    Hud(engine::ITextFactory& textFactory, IPlayerSpriteFactory& playerSpriteFactory);

    void update(engine::Registry& registry);
    void render(engine::IWindow& window) const;

  private:
    struct PlayerLine {
      std::shared_ptr<engine::IText> label;
      std::shared_ptr<engine::IText> score;
      std::shared_ptr<engine::ISprite> lifeIcon;
      std::uint8_t lives{0};
    };

    PlayerLine& getOrCreateLine(network::NetworkId playerId);
    void layoutLines();

    static std::string formatScore(std::uint32_t score);
    static engine::Color withAlpha(engine::Color color, std::uint8_t alpha);

    engine::ITextFactory& _textFactory;
    IPlayerSpriteFactory& _playerSpriteFactory;
    std::map<network::NetworkId, PlayerLine> _lines;
  };
}

#endif // !HUD_HPP
