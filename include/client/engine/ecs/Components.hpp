#ifndef RTYPE_ENGINE_COMPONENTS_HPP
#define RTYPE_ENGINE_COMPONENTS_HPP

#include "engine/gfx/GfxTypes.hpp"
#include <cstdint>
#include <optional>
#include <string>

// Generic engine components: pure data, no backend type, no R-Type logic.

namespace rtype::engine {
  struct TransformComponent {
    Vector2f position{0.0f, 0.0f};
    float rotation{0.0f};
    Vector2f scale{1.0f, 1.0f};
    Vector2f targetPosition{0.0f, 0.0f};  ///< Used by InterpolationSystem.
  };

  struct VelocityComponent {
    Vector2f velocity{0.0f, 0.0f};
  };

  /// What to draw: an asset id from the manifest. The RenderSystem owns the
  /// actual backend sprite, the component never holds one.
  struct SpriteComponent {
    std::string assetId;
    int zIndex{0};                ///< Lower is drawn first (background).
    std::optional<Color> tint{};  ///< Overrides the manifest tint when set.
    bool visible{true};
  };

  /// Text drawn at the entity's TransformComponent position.
  struct TextComponent {
    std::string fontId;
    std::string text;
    unsigned int characterSize{16};
    Color color{255, 255, 255, 255};
    Color outlineColor{0, 0, 0, 255};
    float outlineThickness{0.0f};
    Vector2f origin{0.0f, 0.0f};  ///< Pivot in [0, 1]: {0,0} top-left, {1,0} top-right, {0.5,0.5} center.
    int zIndex{100};
    bool visible{true};
  };

  /// Keeps an entity at a fixed place relative to the window (HUD), whatever the
  /// resolution: position = anchor * windowSize + offset.
  struct ScreenAnchorComponent {
    Vector2f anchor{0.0f, 0.0f};  ///< In [0, 1] of the window size.
    Vector2f offset{0.0f, 0.0f};  ///< In pixels.
  };

  /// Periodically shows / hides the entity's sprite and text.
  struct BlinkComponent {
    float period{1.0f};        ///< Seconds for a full on + off cycle.
    float visibleRatio{0.6f};  ///< Share of the period during which it is visible.
    float elapsed{0.0f};
  };

  /// Hit points, as last reported by the authoritative source (the server).
  struct HealthComponent {
    int current{1};
    int max{1};
  };

  /// Temporarily recolors the sprite (hit feedback), then restores its tint.
  struct FlashComponent {
    Color color{255, 80, 80, 255};
    float duration{0.15f};
    float remaining{0.0f};           ///< > 0 while flashing; set it to `duration` to trigger.
    std::optional<Color> savedTint{};
    bool active{false};
  };

  /// Destroys the entity after `remaining` seconds (notifications, effects).
  struct LifetimeComponent {
    float remaining{1.0f};
  };

  /// Horizontal spritesheet animation. Added automatically when the manifest
  /// declares `frames > 1`.
  struct AnimationComponent {
    int frameCount{1};
    float frameTime{0.1f};
    bool loop{true};
    int currentFrame{0};
    float elapsed{0.0f};
    bool finished{false};
  };

  /// Entity wraps around the screen horizontally (starfield, scrolling decor).
  struct ParallaxComponent {
    float wrapMargin{0.0f};
  };

  /// Smoothly moves the transform toward `targetPosition` (network entities).
  struct InterpolationComponent {
    float speed{15.0f};
  };

  struct NetworkIdComponent {
    std::uint32_t netId{0};
    bool isLocalPlayer{false};
  };
}

#endif // !RTYPE_ENGINE_COMPONENTS_HPP
