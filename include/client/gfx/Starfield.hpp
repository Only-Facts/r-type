#ifndef STARFIELD_HPP
#define STARFIELD_HPP

#include "IBackground.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace rtype::engine {
  class Starfield : public IBackground {
  public:
    Starfield() = default;
    ~Starfield() override = default;

    void update(float deltaTime) override;
    void render(IWindow& window) override;

  private:
    struct Star {
      Vector2f position;
      float speed;
      float size;
      std::uint8_t brightness;
    };

    struct LayerConfig {
      float speed;
      float size;
      std::uint8_t brightness;
      std::size_t starCount;
    };

    static constexpr float REFERENCE_WINDOW_HEIGHT = 1080.0f;
    static constexpr std::array<LayerConfig, 3> LAYERS{{
      {30.0f, 2.0f, 90, 120},
      {80.0f, 3.0f, 170, 70},
      {180.0f, 4.0f, 255, 35},
    }};

    void generateStars(const Vector2u& windowSize);
    void scrollStar(Star& star, float deltaTime);
    float randomX();
    float randomY();

    std::vector<Star> _stars;
    std::mt19937 _randomEngine{std::random_device{}()};
    Vector2u _windowSize{0, 0};
    bool _hasStars{false};
  };
}

#endif // !STARFIELD_HPP
