#include "MissileManager.hpp"
#include "GameConstants.hpp"

namespace rtype::server {
  Missile MissileManager::spawn(network::NetworkId ownerId, float posX, float posY) {
    const Missile missile{_nextMissileId++, ownerId, posX, posY, game::MISSILE_SPEED, 0.0f};
    _missiles[missile.id] = missile;
    return missile;
  }

  std::vector<network::NetworkId> MissileManager::update(float deltaTime) {
    std::vector<network::NetworkId> destroyedIds;

    for (auto& [id, missile] : _missiles) {
      missile.posX += missile.velX * deltaTime;
      missile.posY += missile.velY * deltaTime;
      if (isOutsideWorld(missile)) destroyedIds.push_back(id);
    }

    for (network::NetworkId id : destroyedIds)
      _missiles.erase(id);
    return destroyedIds;
  }

  const std::unordered_map<network::NetworkId, Missile>& MissileManager::getMissiles() const {
    return _missiles;
  }

  bool MissileManager::isOutsideWorld(const Missile& missile) const {
    const float margin = game::WORLD_DESPAWN_MARGIN;
    return missile.posX < -margin || missile.posX > game::WORLD_WIDTH + margin
        || missile.posY < -margin || missile.posY > game::WORLD_HEIGHT + margin;
  }
}
