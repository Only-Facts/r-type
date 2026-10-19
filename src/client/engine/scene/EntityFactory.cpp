#include "engine/ecs/EntityFactory.hpp"
#include "engine/ecs/Components.hpp"

namespace rtype::engine {
  Entity EntityFactory::createSprite(const std::string& assetId, const Vector2f& position, int zIndex) {
    const Entity entity = _registry.createEntity();
    TransformComponent transform;
    transform.position = position;
    transform.targetPosition = position;
    _registry.addComponent(entity, transform);
    attachSprite(entity, assetId, zIndex);
    return entity;
  }

  void EntityFactory::attachSprite(Entity entity, const std::string& assetId, int zIndex) {
    const AssetDescriptor& desc = _resources.get(assetId);

    if (!_registry.getComponent<TransformComponent>(entity))
      _registry.addComponent(entity, TransformComponent{});

    SpriteComponent sprite;
    sprite.assetId = assetId;
    sprite.zIndex = zIndex;
    _registry.addComponent(entity, sprite);

    if (desc.frames > 1) {
      AnimationComponent anim;
      anim.frameCount = desc.frames;
      anim.frameTime = desc.frameTime;
      anim.loop = desc.loop;
      _registry.addComponent(entity, anim);
    }
  }
}
