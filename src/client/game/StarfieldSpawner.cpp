#include "game/StarfieldSpawner.hpp"
#include "engine/ecs/Components.hpp"
#include "exception.hpp"
#include <algorithm>
#include <cstddef>
#include <random>

namespace rtype::game {
  StarfieldSpawner::Settings StarfieldSpawner::loadSettings(const engine::Config& config, const std::string& section) {
    Settings s;
    s.starAsset = config.getString(section, "star_asset", s.starAsset);
    s.starCount = config.getInt(section, "star_count", s.starCount);
    s.starZ = config.getInt(section, "star_z", s.starZ);
    if (config.has(section, "layer_speeds")) s.layerSpeeds = config.getFloatList(section, "layer_speeds");
    if (config.has(section, "layer_scales")) s.layerScales = config.getFloatList(section, "layer_scales");
    if (config.has(section, "layer_alphas")) s.layerAlphas = config.getIntList(section, "layer_alphas");

    s.planetAsset = config.getString(section, "planet_asset", s.planetAsset);
    s.planetCount = config.getInt(section, "planet_count", s.planetCount);
    s.planetZ = config.getInt(section, "planet_z", s.planetZ);
    s.planetBaseSpeed = config.getFloat(section, "planet_base_speed", s.planetBaseSpeed);
    s.planetSpeedStep = config.getFloat(section, "planet_speed_step", s.planetSpeedStep);
    s.planetBaseScale = config.getFloat(section, "planet_base_scale", s.planetBaseScale);
    s.planetScaleStep = config.getFloat(section, "planet_scale_step", s.planetScaleStep);
    s.planetAlpha = config.getInt(section, "planet_alpha", s.planetAlpha);

    s.wrapMargin = config.getFloat(section, "wrap_margin", s.wrapMargin);
    s.seed = static_cast<std::uint32_t>(std::max(0, config.getInt(section, "seed", 0)));

    const std::size_t layers = s.layerSpeeds.size();
    if (layers == 0 || s.layerScales.size() != layers || s.layerAlphas.size() != layers)
      throw RtypeError("[" + section + "] layer_speeds, layer_scales and layer_alphas must have the same, non-zero, length");
    if (s.starCount < 0 || s.planetCount < 0)
      throw RtypeError("[" + section + "] star_count and planet_count must be >= 0");

    auto alphaOk = [](int a) { return a >= 0 && a <= 255; };
    if (!alphaOk(s.planetAlpha) || !std::all_of(s.layerAlphas.begin(), s.layerAlphas.end(), alphaOk))
      throw RtypeError("[" + section + "] alpha values must be in [0, 255]");
    return s;
  }

  void StarfieldSpawner::spawn(const Settings& settings, const engine::Vector2u& area) {
    std::mt19937 rng{settings.seed != 0 ? settings.seed : std::random_device{}()};
    std::uniform_real_distribution<float> randomX(0.0f, static_cast<float>(area.x));
    std::uniform_real_distribution<float> randomY(0.0f, static_cast<float>(area.y));
    std::uniform_int_distribution<std::size_t> randomLayer(0, settings.layerSpeeds.size() - 1);

    for (int i = 0; i < settings.planetCount; ++i) {
      const float step = static_cast<float>(i);
      spawnLayerEntity(settings.planetAsset, {randomX(rng), randomY(rng)}, settings.planetZ,
                       settings.planetBaseSpeed + settings.planetSpeedStep * step,
                       settings.planetBaseScale + settings.planetScaleStep * step,
                       settings.planetAlpha, settings.wrapMargin);
    }

    for (int i = 0; i < settings.starCount; ++i) {
      const std::size_t layer = randomLayer(rng);
      spawnLayerEntity(settings.starAsset, {randomX(rng), randomY(rng)},
                       settings.starZ + static_cast<int>(layer),
                       settings.layerSpeeds[layer], settings.layerScales[layer],
                       settings.layerAlphas[layer], settings.wrapMargin);
    }
  }

  void StarfieldSpawner::spawnLayerEntity(const std::string& asset, const engine::Vector2f& position, int z,
                                          float speed, float scale, int alpha, float wrapMargin) {
    const engine::Entity entity = _factory.createSprite(asset, position, z);

    if (auto* transform = _registry.getComponent<engine::TransformComponent>(entity))
      transform->scale = {scale, scale};
    if (auto* sprite = _registry.getComponent<engine::SpriteComponent>(entity))
      sprite->tint = engine::Color{255, 255, 255, static_cast<std::uint8_t>(alpha)};

    engine::VelocityComponent velocity;
    velocity.velocity = {-speed, 0.0f};
    _registry.addComponent(entity, velocity);

    engine::ParallaxComponent parallax;
    parallax.wrapMargin = wrapMargin;
    _registry.addComponent(entity, parallax);
  }
}
