#include "game/MessageOverlay.hpp"

namespace rtype::client {
  namespace {
    constexpr unsigned int TITLE_SIZE = 80;
    constexpr unsigned int SUBTITLE_SIZE = 36;
    constexpr unsigned int HINT_SIZE = 24;

    constexpr float TITLE_OFFSET_Y = -110.0f;
    constexpr float SUBTITLE_OFFSET_Y = 0.0f;
    constexpr float HINT_OFFSET_Y = 70.0f;

    constexpr engine::Color BACKDROP_COLOR{0, 0, 0, 160};
    constexpr engine::Color TITLE_COLOR{255, 220, 90, 255};
    constexpr engine::Color SUBTITLE_COLOR{255, 255, 255, 255};
    constexpr engine::Color HINT_COLOR{170, 170, 170, 255};
  }

  MessageOverlay::MessageOverlay(engine::ITextFactory& textFactory)
    : _title(textFactory.createText(TITLE_SIZE)),
      _subtitle(textFactory.createText(SUBTITLE_SIZE)),
      _hint(textFactory.createText(HINT_SIZE)) {
    _title->setColor(TITLE_COLOR);
    _subtitle->setColor(SUBTITLE_COLOR);
    _hint->setColor(HINT_COLOR);
  }

  void MessageOverlay::setMessage(const std::string& title, const std::string& subtitle, const std::string& hint) {
    _title->setString(title);
    _subtitle->setString(subtitle);
    _hint->setString(hint);
  }

  void MessageOverlay::render(engine::IWindow& window) const {
    const engine::Vector2u size = window.getSize();
    const float width = static_cast<float>(size.x);
    const float centerY = static_cast<float>(size.y) / 2.0f;

    window.drawRectangle({0.0f, 0.0f}, {width, static_cast<float>(size.y)}, BACKDROP_COLOR);

    centerHorizontally(*_title, width, centerY + TITLE_OFFSET_Y);
    centerHorizontally(*_subtitle, width, centerY + SUBTITLE_OFFSET_Y);
    centerHorizontally(*_hint, width, centerY + HINT_OFFSET_Y);

    window.draw(*_title);
    window.draw(*_subtitle);
    window.draw(*_hint);
  }

  void MessageOverlay::centerHorizontally(engine::IText& text, float windowWidth, float y) {
    text.setPosition({(windowWidth - text.getSize().x) / 2.0f, y});
  }
}
