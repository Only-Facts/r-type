#ifndef STARFIELD_HPP
#define STARFIELD_HPP

#include "IBackground.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

namespace rtype::engine {
  // Scrolling parallax background made of three layers of stars.
  // Scrolling depends on the elapsed time only, never on the CPU speed.
  class Starfield : public IBackground {
  public:
    Starfield() = default;
    ~Starfield() override = default;

    void update(float deltaTime) override;
    void render(IWindow& window) override;

  private:
    struct Star {
      Vector2f position;
      float speed;       // pixels per second
      float size;        // pixels
      std::uint8_t brightness;
    };

    struct LayerConfig {
      float speed;       // pixels per second on a 1080p window
      float size;        // pixels on a 1080p window
      std::uint8_t brightness;
      std::size_t starCount;
    };

    static constexpr float REFERENCE_WINDOW_HEIGHT = 1080.0f;
    static constexpr std::array<LayerConfig, 3> LAYERS{{
      {30.0f, 2.0f, 90, 120},   // far layer: slow and dim
      {80.0f, 3.0f, 170, 70},   // middle layer
      {180.0f, 4.0f, 255, 35},  // near layer: fast and bright
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
