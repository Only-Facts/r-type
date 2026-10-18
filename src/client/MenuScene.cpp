#include "MenuScene.hpp"
#include "gfx/Starfield.hpp"
#include <cctype>
#include <limits>

namespace rtype::engine {
  namespace {
    constexpr unsigned int TITLE_SIZE = 140;
    constexpr unsigned int SUBTITLE_SIZE = 28;
    constexpr unsigned int ITEM_SIZE = 44;
    constexpr unsigned int ERROR_SIZE = 26;
    constexpr unsigned int HINT_SIZE = 22;

    constexpr float TITLE_Y_RATIO = 0.16f;
    constexpr float FIRST_ITEM_Y_RATIO = 0.45f;
    constexpr float ITEM_SPACING = 70.0f;
    constexpr float ITEM_COLUMN_WIDTH = 420.0f;
    constexpr float SELECTION_MARKER_OFFSET = 50.0f;
    constexpr float HINT_BOTTOM_MARGIN = 60.0f;

    constexpr std::size_t MAX_HOST_LENGTH = 64;
    constexpr std::size_t MAX_PORT_LENGTH = 5;
    constexpr float CURSOR_BLINK_PERIOD = 0.5f;

    constexpr Color TITLE_COLOR{255, 220, 90, 255};
    constexpr Color SUBTITLE_COLOR{150, 170, 220, 255};
    constexpr Color SELECTED_COLOR{255, 255, 255, 255};
    constexpr Color UNSELECTED_COLOR{130, 130, 150, 255};
    constexpr Color ERROR_COLOR{255, 100, 90, 255};
    constexpr Color HINT_COLOR{150, 150, 150, 255};

    bool isHostCharacter(char character) {
      return std::isalnum(static_cast<unsigned char>(character)) || character == '.' || character == '-' || character == ':';
    }

    void centerHorizontally(IText& text, float windowWidth, float y) {
      text.setPosition({(windowWidth - text.getSize().x) / 2.0f, y});
    }
  }

  MenuScene::MenuScene(IInputManager& inputMgr,
                       ITextFactory& textFactory,
                       std::string defaultHost,
                       unsigned short defaultPort,
                       StartGameCallback onStartGame,
                       std::function<void()> onQuit)
    : _inputMgr(inputMgr),
      _background(std::make_unique<Starfield>()),
      _onStartGame(std::move(onStartGame)),
      _onQuit(std::move(onQuit)),
      _title(textFactory.createText(TITLE_SIZE)),
      _subtitle(textFactory.createText(SUBTITLE_SIZE)),
      _selectionMarker(textFactory.createText(ITEM_SIZE)),
      _error(textFactory.createText(ERROR_SIZE)),
      _hint(textFactory.createText(HINT_SIZE)),
      _host(std::move(defaultHost)),
      _port(std::to_string(defaultPort)) {
    for (auto& item : _items)
      item = textFactory.createText(ITEM_SIZE);

    _title->setString("R-TYPE");
    _title->setColor(TITLE_COLOR);
    _subtitle->setString("MULTIPLAYER");
    _subtitle->setColor(SUBTITLE_COLOR);
    _selectionMarker->setString(">");
    _selectionMarker->setColor(TITLE_COLOR);
    _error->setColor(ERROR_COLOR);
    _hint->setString("UP / DOWN  select     TYPE  edit     ENTER  confirm     ESC  quit");
    _hint->setColor(HINT_COLOR);
  }

  void MenuScene::init() {
    _selected = Item::Play;
    _error->setString("");
    refreshItemTexts();
  }

  void MenuScene::update(float dt) {
    _background->update(dt);
    _cursorTimer += dt;

    if (_inputMgr.isKeyJustPressed(KeyCode::Escape)) {
      if (_onQuit) _onQuit();
      return;
    }

    if (_inputMgr.isKeyJustPressed(KeyCode::Up)) moveSelection(-1);
    if (_inputMgr.isKeyJustPressed(KeyCode::Down) || _inputMgr.isKeyJustPressed(KeyCode::Tab)) moveSelection(1);

    editSelectedField(_inputMgr.getTextEntered());

    if (_inputMgr.isKeyJustPressed(KeyCode::Enter)) confirm();

    refreshItemTexts();
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

    const float itemX = (width - ITEM_COLUMN_WIDTH) / 2.0f;
    float itemY = height * FIRST_ITEM_Y_RATIO;
    for (std::size_t i = 0; i < ITEM_COUNT; ++i) {
      _items[i]->setPosition({itemX, itemY});
      window.draw(*_items[i]);

      if (static_cast<Item>(i) == _selected) {
        _selectionMarker->setPosition({itemX - SELECTION_MARKER_OFFSET, itemY});
        window.draw(*_selectionMarker);
      }
      itemY += ITEM_SPACING;
    }

    centerHorizontally(*_error, width, itemY + ITEM_SPACING * 0.25f);
    window.draw(*_error);

    centerHorizontally(*_hint, width, height - HINT_BOTTOM_MARGIN);
    window.draw(*_hint);
  }

  void MenuScene::moveSelection(int offset) {
    const int count = static_cast<int>(ITEM_COUNT);
    const int index = (static_cast<int>(_selected) + offset + count) % count;
    _selected = static_cast<Item>(index);
    _cursorTimer = 0.0f;
  }

  void MenuScene::editSelectedField(const std::string& typedText) {
    if (!isEditable(_selected) || typedText.empty()) return;

    std::string& field = _selected == Item::Host ? _host : _port;
    const std::size_t maxLength = _selected == Item::Host ? MAX_HOST_LENGTH : MAX_PORT_LENGTH;

    for (char character : typedText) {
      if (character == BACKSPACE_CHARACTER) {
        if (!field.empty()) field.pop_back();
        continue;
      }

      const bool isAllowed = _selected == Item::Host
        ? isHostCharacter(character)
        : std::isdigit(static_cast<unsigned char>(character)) != 0;
      if (isAllowed && field.size() < maxLength) field.push_back(character);
    }

    _error->setString("");
    _cursorTimer = 0.0f;
  }

  void MenuScene::confirm() {
    if (_selected == Item::Quit) {
      if (_onQuit) _onQuit();
      return;
    }

    if (_host.empty()) {
      _error->setString("Host cannot be empty");
      _selected = Item::Host;
      return;
    }

    const std::optional<unsigned short> port = parsePort();
    if (!port) {
      _error->setString("Port must be a number between 1 and 65535");
      _selected = Item::Port;
      return;
    }

    if (_onStartGame) _onStartGame(_host, *port);
  }

  std::optional<unsigned short> MenuScene::parsePort() const {
    if (_port.empty()) return std::nullopt;

    const unsigned long value = std::stoul(_port);
    if (value == 0 || value > std::numeric_limits<unsigned short>::max()) return std::nullopt;
    return static_cast<unsigned short>(value);
  }

  void MenuScene::refreshItemTexts() {
    const bool showCursor = static_cast<int>(_cursorTimer / CURSOR_BLINK_PERIOD) % 2 == 0;

    for (std::size_t i = 0; i < ITEM_COUNT; ++i) {
      const auto item = static_cast<Item>(i);
      const bool isSelected = item == _selected;

      std::string label;
      switch (item) {
        case Item::Host: label = "HOST   " + _host; break;
        case Item::Port: label = "PORT   " + _port; break;
        case Item::Play: label = "PLAY"; break;
        case Item::Quit: label = "QUIT"; break;
        case Item::Count: break;
      }

      if (isSelected && isEditable(item) && showCursor) label += "_";

      _items[i]->setString(label);
      _items[i]->setColor(isSelected ? SELECTED_COLOR : UNSELECTED_COLOR);
    }
  }

  bool MenuScene::isEditable(Item item) const {
    return item == Item::Host || item == Item::Port;
  }
}
