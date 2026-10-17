#ifndef MISSILEMANAGER_HPP
#define MISSILEMANAGER_HPP

#include "GameConstants.hpp"
#include "network/Protocol.hpp"
#include <unordered_map>
#include <vector>

namespace rtype::server {
  struct Missile {
    network::NetworkId id;
    network::NetworkId ownerId;
    float posX;
    float posY;
    float velX;
    float velY;
  };

  class MissileManager {
  public:
    Missile spawn(network::NetworkId ownerId, float posX, float posY);

    std::vector<network::NetworkId> update(float deltaTime);

    void remove(network::NetworkId missileId);

    const std::unordered_map<network::NetworkId, Missile>& getMissiles() const;

    static game::Hitbox getHitbox(const Missile& missile);

  private:
    bool isOutsideWorld(const Missile& missile) const;

    std::unordered_map<network::NetworkId, Missile> _missiles;
    network::NetworkId _nextMissileId{game::FIRST_PROJECTILE_ID};
  };
}

#endif // !MISSILEMANAGER_HPP
