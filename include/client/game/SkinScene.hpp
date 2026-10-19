#ifndef RTYPE_GAME_SKINSCENE_HPP
#define RTYPE_GAME_SKINSCENE_HPP

#include "engine/core/EngineContext.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include "engine/scene/AScene.hpp"
#include "game/SkinCatalog.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rtype::game {
  class SkinScene : public engine::AScene {
  public:
    explicit SkinScene(engine::EngineContext& context);

  protected:
    void onInit() override;
    void onUpdate(float dt) override;

  private:
    engine::Entity addText(const std::string& text, float anchorY, unsigned int size, const engine::Color& color);
    void change(int step);
    void refresh();
    void playConfigSound(const std::string& key);

    SkinCatalog _skins;
    std::vector<engine::Entity> _ships;
    engine::Entity _label{0};
    std::string _font;
    std::string _prevAction;
    std::string _nextAction;
    std::string _startAction;
    std::string _backAction;
    std::string _nextScene;
    std::string _previousScene;
    float _carouselY;
    float _spacing;
    float _selectedScale;
    float _otherScale;
    std::uint8_t _otherAlpha;
    float _outlineThickness;
  };
}

#endif // !RTYPE_GAME_SKINSCENE_HPP
