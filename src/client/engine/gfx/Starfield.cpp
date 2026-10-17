#include "gfx/Starfield.hpp"

namespace rtype::engine {
  void Starfield::update(float deltaTime) {
    for (auto& star : _stars)
      scrollStar(star, deltaTime);
  }

  void Starfield::render(IWindow& window) {
    const Vector2u windowSize = window.getSize();
    if (!_hasStars || windowSize.x != _windowSize.x || windowSize.y != _windowSize.y)
      generateStars(windowSize);

    for (const auto& star : _stars) {
      const Color color{star.brightness, star.brightness, star.brightness, 255};
      window.drawRectangle(star.position, {star.size, star.size}, color);
    }
  }

  void Starfield::generateStars(const Vector2u& windowSize) {
    _windowSize = windowSize;
    _stars.clear();

    const float scale = static_cast<float>(windowSize.y) / REFERENCE_WINDOW_HEIGHT;

    for (const auto& layer : LAYERS) {
      for (std::size_t i = 0; i < layer.starCount; ++i)
        _stars.push_back({{randomX(), randomY()}, layer.speed * scale, layer.size * scale, layer.brightness});
    }
    _hasStars = true;
  }

  void Starfield::scrollStar(Star& star, float deltaTime) {
    star.position.x -= star.speed * deltaTime;

    if (star.position.x + star.size < 0.0f) {
      star.position.x += static_cast<float>(_windowSize.x) + star.size;
      star.position.y = randomY();
    }
  }

  float Starfield::randomX() {
    return std::uniform_real_distribution<float>(0.0f, static_cast<float>(_windowSize.x))(_randomEngine);
  }

  float Starfield::randomY() {
    return std::uniform_real_distribution<float>(0.0f, static_cast<float>(_windowSize.y))(_randomEngine);
  }
}
