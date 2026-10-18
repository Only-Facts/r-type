#ifndef RTYPE_GAME_TITLESCENE_HPP
#define RTYPE_GAME_TITLESCENE_HPP

#include "engine/core/EngineContext.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include "engine/scene/AScene.hpp"
#include <string>

namespace rtype::game {
  /// Title screen: star-field, title, blinking "press start", server address and a
  /// controls list generated from the live [input] bindings (remapped keys show up
  /// automatically). Every text, size and color comes from the [title] section.
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

    std::string _font;
    std::string _startAction;
    std::string _nextScene;
    float _outlineThickness;
  };
}

#endif // !RTYPE_GAME_TITLESCENE_HPP
