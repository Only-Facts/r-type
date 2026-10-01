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
    void clear() override;
    void display() override;
    void pollEvents(IInputManager& inputManager) override;
    float getDeltaTime() override;

  private:
    sf::RenderWindow _window;
    sf::Clock _clock;
    float _deltaTime;
  };
}

#endif // !SFMLWINDOW_HPP
