#ifndef RTYPE_GAME_STARFIELDSPAWNER_HPP
#define RTYPE_GAME_STARFIELDSPAWNER_HPP

#include "engine/config/Config.hpp"
#include "engine/ecs/EntityFactory.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace rtype::game {
  /// Builds the scrolling star-field as plain ECS entities
  /// (Sprite + Transform + Velocity + Parallax). Every value comes from the config;
  /// the engine systems (Movement, Parallax, Render) do the rest.
  class StarfieldSpawner {
  public:
    struct Settings {
      std::string starAsset{"star"};
      int starCount{150};
      int starZ{-20};
      std::vector<float> layerSpeeds{40.0f, 100.0f, 220.0f};
      std::vector<float> layerScales{1.0f, 2.0f, 3.0f};
      std::vector<int> layerAlphas{110, 180, 255};

      std::string planetAsset{"planet"};
      int planetCount{3};
      int planetZ{-30};
      float planetBaseSpeed{12.0f};
      float planetSpeedStep{8.0f};
      float planetBaseScale{1.5f};
      float planetScaleStep{1.0f};
      int planetAlpha{90};

      float wrapMargin{300.0f};
      std::uint32_t seed{0};  ///< 0 = random each launch.
    };

    static Settings loadSettings(const engine::Config& config, const std::string& section);

    StarfieldSpawner(engine::Registry& registry, engine::EntityFactory& factory)
      : _registry(registry), _factory(factory) {}

    void spawn(const Settings& settings, const engine::Vector2u& area);

  private:
    void spawnLayerEntity(const std::string& asset, const engine::Vector2f& position, int z,
                          float speed, float scale, int alpha, float wrapMargin);

    engine::Registry& _registry;
    engine::EntityFactory& _factory;
  };
}

#endif // !RTYPE_GAME_STARFIELDSPAWNER_HPP
