#ifndef RTYPE_GAME_TITLESCENE_HPP
#define RTYPE_GAME_TITLESCENE_HPP

#include "engine/core/EngineContext.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include "engine/scene/AScene.hpp"
#include "game/Difficulty.hpp"
#include <cstddef>
#include <string>

namespace rtype::game {

  class TitleScene : public engine::AScene {
  public:
    explicit TitleScene(engine::EngineContext& context);

  protected:
    void onInit() override;
    void onUpdate(float dt) override;

  private:
    engine::Entity addText(const std::string& text, float anchorY, float offsetY, unsigned int size,
                           const engine::Color& color);
    std::string keysFor(const std::string& action) const;
    void changeDifficulty(int step);
    void refreshDifficulty();

    std::string _font;
    std::string _startAction;
    std::string _nextScene;
    float _outlineThickness;
    Difficulty _difficulty;
    std::string _prevAction;
    std::string _nextAction;
    engine::Entity _difficultyText{0};
  };
}

#endif // !RTYPE_GAME_TITLESCENE_HPP
