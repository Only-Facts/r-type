#ifndef RTYPE_ENGINE_COMPONENTS_HPP
#define RTYPE_ENGINE_COMPONENTS_HPP

#include "engine/gfx/GfxTypes.hpp"
#include <cstdint>
#include <optional>
#include <string>


namespace rtype::engine {
  struct TransformComponent {
    Vector2f position{0.0f, 0.0f};
    float rotation{0.0f};
    Vector2f scale{1.0f, 1.0f};
    Vector2f targetPosition{0.0f, 0.0f}; 
  };

  struct VelocityComponent {
    Vector2f velocity{0.0f, 0.0f};
  };
  struct SpriteComponent {
    std::string assetId;
    int zIndex{0};                
    std::optional<Color> tint{};  
    bool visible{true};
  };

  struct TextComponent {
    std::string fontId;
    std::string text;
    unsigned int characterSize{16};
    Color color{255, 255, 255, 255};
    Color outlineColor{0, 0, 0, 255};
    float outlineThickness{0.0f};
    Vector2f origin{0.0f, 0.0f}; 
    int zIndex{100};
    bool visible{true};
  };


  struct ScreenAnchorComponent {
    Vector2f anchor{0.0f, 0.0f};  
    Vector2f offset{0.0f, 0.0f};  
  };

  struct BlinkComponent {
    float period{1.0f};        
    float visibleRatio{0.6f};  
    float elapsed{0.0f};
  };

  struct HealthComponent {
    int current{1};
    int max{1};
  };

  struct FlashComponent {
    Color color{255, 80, 80, 255};
    float duration{0.15f};
    float remaining{0.0f};         
    std::optional<Color> savedTint{};
    bool active{false};
  };

  struct LifetimeComponent {
    float remaining{1.0f};
    float duration{0.0f};
    bool fadeOut{false};
    float fadePortion{1.0f};
  };


  struct AnimationComponent {
    int frameCount{1};
    float frameTime{0.1f};
    bool loop{true};
    int currentFrame{0};
    float elapsed{0.0f};
    bool finished{false};
  };

  struct ParallaxComponent {
    float wrapMargin{0.0f};
  };

  struct InterpolationComponent {
    float speed{15.0f};
  };

  struct NetworkIdComponent {
    std::uint32_t netId{0};
    bool isLocalPlayer{false};
  };
}

#endif // !RTYPE_ENGINE_COMPONENTS_HPP
