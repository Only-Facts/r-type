#ifndef SFMLWINDOW_HPP
#define SFMLWINDOW_HPP

#include "IWindow.hpp"
#include <SFML/Graphics.hpp>

namespace rtype::engine {
  class SFMLWindow : public IWindow {
  public:
    SFMLWindow(unsigned int width, unsigned int height, const std::string& title);
    ~SFMLWindow() override = default;

    bool isOpen() const override;
    void close() override;
    void pollEvents(IInputManager& inputManager) override;
    float getDeltaTime() override;
    Vector2u getSize() const override;

    void clear(const Color& color = Color{0, 0, 0, 255}) override;
    void draw(const ISprite& sprite) override;
    void drawRectangle(const Vector2f& position, const Vector2f& size, const Color& color) override;
    void display() override;

  private:
    sf::RenderWindow _window;
    sf::Clock _clock;
    float _deltaTime{0.0f};
  };
}

#endif // !SFMLWINDOW_HPP
