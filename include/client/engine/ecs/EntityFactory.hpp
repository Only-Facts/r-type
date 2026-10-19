#ifndef RTYPE_ENGINE_ENTITYFACTORY_HPP
#define RTYPE_ENGINE_ENTITYFACTORY_HPP

#include "engine/ecs/Registry.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include "engine/resources/ResourceManager.hpp"
#include <string>

namespace rtype::engine {
  class EntityFactory {
  public:
    EntityFactory(Registry& registry, const ResourceManager& resources)
      : _registry(registry), _resources(resources) {}

    Entity createSprite(const std::string& assetId, const Vector2f& position, int zIndex = 0);
    void attachSprite(Entity entity, const std::string& assetId, int zIndex = 0);

  private:
    Registry& _registry;
    const ResourceManager& _resources;
  };
}

#endif // !RTYPE_ENGINE_ENTITYFACTORY_HPP
