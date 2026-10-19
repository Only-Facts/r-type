#ifndef MENUSCENE_HPP
#define MENUSCENE_HPP

#include "scene/IScene.hpp"
#include "gfx/IBackground.hpp"
#include "gfx/ITextFactory.hpp"
#include "input/IInputManager.hpp"
#include <array>
#include <functional>
#include <memory>

namespace rtype::engine {
  class MenuScene : public IScene {
  public:
    MenuScene(IInputManager& inputMgr,
              ITextFactory& textFactory,
              std::function<void()> onPlay,
              std::function<void()> onQuit);
    ~MenuScene() override = default;

    void init() override;
    void update(float dt) override;
    void render(IWindow& window) override;

  private:
    enum class Button { Play, Quit, Count };

    static constexpr std::size_t BUTTON_COUNT = static_cast<std::size_t>(Button::Count);

    void moveSelection(int offset);
    void activateSelectedButton();
    void quit();

    IInputManager& _inputMgr;
    std::unique_ptr<IBackground> _background;
    std::function<void()> _onPlay;
    std::function<void()> _onQuit;

    std::shared_ptr<IText> _title;
    std::shared_ptr<IText> _subtitle;
    std::array<std::shared_ptr<IText>, BUTTON_COUNT> _buttons;
    std::shared_ptr<IText> _selectionMarker;
    std::shared_ptr<IText> _hint;

    Button _selected{Button::Play};
  };
}

#endif // !MENUSCENE_HPP
