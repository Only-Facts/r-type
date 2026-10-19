#ifndef RTYPE_BACKEND_SFMLWINDOW_HPP
#define RTYPE_BACKEND_SFMLWINDOW_HPP

#include "engine/platform/IWindow.hpp"
#include "engine/platform/WindowConfig.hpp"
#include <SFML/Graphics/RenderWindow.hpp>

namespace rtype::backend {
  class SFMLWindow : public engine::IWindow {
  public:
    explicit SFMLWindow(const engine::WindowConfig& config);
    ~SFMLWindow() override = default;

    bool isOpen() const override;
    void close() override;
    engine::Vector2u getSize() const override;

    void clear(const engine::Color& color) override;
    void draw(const engine::ISprite& sprite) override;
    void draw(const engine::IText& text) override;
    void display() override;

    sf::RenderWindow& getNative() { return _window; }

  private:
    sf::RenderWindow _window;
  };
}

#endif // !RTYPE_BACKEND_SFMLWINDOW_HPP
