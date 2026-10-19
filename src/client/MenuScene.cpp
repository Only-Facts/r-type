#include "MenuScene.hpp"
#include "gfx/Starfield.hpp"

namespace rtype::engine {
  namespace {
    constexpr unsigned int TITLE_SIZE = 140;
    constexpr unsigned int SUBTITLE_SIZE = 28;
    constexpr unsigned int BUTTON_SIZE = 44;
    constexpr unsigned int HINT_SIZE = 22;

    constexpr float TITLE_Y_RATIO = 0.16f;
    constexpr float FIRST_BUTTON_Y_RATIO = 0.45f;
    constexpr float BUTTON_SPACING = 70.0f;
    constexpr float SELECTION_MARKER_OFFSET = 50.0f;
    constexpr float HINT_BOTTOM_MARGIN = 60.0f;

    constexpr Color TITLE_COLOR{255, 220, 90, 255};
    constexpr Color SUBTITLE_COLOR{150, 170, 220, 255};
    constexpr Color SELECTED_COLOR{255, 255, 255, 255};
    constexpr Color UNSELECTED_COLOR{130, 130, 150, 255};
    constexpr Color HINT_COLOR{150, 150, 150, 255};

    constexpr std::array<const char*, 2> BUTTON_LABELS{{"PLAY", "QUIT"}};

    void centerHorizontally(IText& text, float windowWidth, float y) {
      text.setPosition({(windowWidth - text.getSize().x) / 2.0f, y});
    }
  }

  MenuScene::MenuScene(IInputManager& inputMgr,
                       ITextFactory& textFactory,
                       std::function<void()> onPlay,
                       std::function<void()> onQuit)
    : _inputMgr(inputMgr),
      _background(std::make_unique<Starfield>()),
      _onPlay(std::move(onPlay)),
      _onQuit(std::move(onQuit)),
      _title(textFactory.createText(TITLE_SIZE)),
      _subtitle(textFactory.createText(SUBTITLE_SIZE)),
      _selectionMarker(textFactory.createText(BUTTON_SIZE)),
      _hint(textFactory.createText(HINT_SIZE)) {
    for (std::size_t i = 0; i < BUTTON_COUNT; ++i) {
      _buttons[i] = textFactory.createText(BUTTON_SIZE);
      _buttons[i]->setString(BUTTON_LABELS[i]);
    }

    _title->setString("R-TYPE");
    _title->setColor(TITLE_COLOR);
    _subtitle->setString("MULTIPLAYER");
    _subtitle->setColor(SUBTITLE_COLOR);
    _selectionMarker->setString(">");
    _selectionMarker->setColor(TITLE_COLOR);
    _hint->setString("UP / DOWN  select     ENTER  confirm     Q / ESC  quit");
    _hint->setColor(HINT_COLOR);
  }

  void MenuScene::init() {
    _selected = Button::Play;
  }

  void MenuScene::update(float dt) {
    _background->update(dt);

    if (_inputMgr.isKeyJustPressed(KeyCode::Escape) || _inputMgr.isKeyJustPressed(KeyCode::KeyQ)) {
      quit();
      return;
    }

    if (_inputMgr.isKeyJustPressed(KeyCode::Up)) moveSelection(-1);
    if (_inputMgr.isKeyJustPressed(KeyCode::Down)) moveSelection(1);

    if (_inputMgr.isKeyJustPressed(KeyCode::Enter)) activateSelectedButton();
  }

  void MenuScene::render(IWindow& window) {
    _background->render(window);

    const Vector2u size = window.getSize();
    const float width = static_cast<float>(size.x);
    const float height = static_cast<float>(size.y);

    const float titleY = height * TITLE_Y_RATIO;
    centerHorizontally(*_title, width, titleY);
    centerHorizontally(*_subtitle, width, titleY + static_cast<float>(TITLE_SIZE) * 1.1f);
    window.draw(*_title);
    window.draw(*_subtitle);

    float buttonY = height * FIRST_BUTTON_Y_RATIO;
    for (std::size_t i = 0; i < BUTTON_COUNT; ++i) {
      IText& button = *_buttons[i];
      const bool isSelected = static_cast<Button>(i) == _selected;

      button.setColor(isSelected ? SELECTED_COLOR : UNSELECTED_COLOR);
      centerHorizontally(button, width, buttonY);
      window.draw(button);

      if (isSelected) {
        const float buttonX = (width - button.getSize().x) / 2.0f;
        _selectionMarker->setPosition({buttonX - SELECTION_MARKER_OFFSET, buttonY});
        window.draw(*_selectionMarker);
      }
      buttonY += BUTTON_SPACING;
    }

    centerHorizontally(*_hint, width, height - HINT_BOTTOM_MARGIN);
    window.draw(*_hint);
  }

  void MenuScene::moveSelection(int offset) {
    const int count = static_cast<int>(BUTTON_COUNT);
    const int index = (static_cast<int>(_selected) + offset + count) % count;
    _selected = static_cast<Button>(index);
  }

  void MenuScene::activateSelectedButton() {
    switch (_selected) {
      case Button::Play:
        if (_onPlay) _onPlay();
        break;
      case Button::Quit:
        quit();
        break;
      case Button::Count:
        break;
    }
  }

  void MenuScene::quit() {
    if (_onQuit) _onQuit();
  }
}
