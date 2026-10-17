#ifndef ENEMYMANAGER_HPP
#define ENEMYMANAGER_HPP

#include "GameConstants.hpp"
#include "network/Protocol.hpp"
#include <random>
#include <unordered_map>

namespace rtype::server {
  struct Enemy {
    network::NetworkId id;
    game::EnemyType type;
    float posX;
    float posY;
    float baseY;
    float age;
  };

  class EnemyManager {
  public:
    EnemyManager();

    void update(float deltaTime);

    const std::unordered_map<network::NetworkId, Enemy>& getEnemies() const;

  private:
    void spawnRandomEnemy();
    void scheduleNextSpawn();
    void moveEnemy(Enemy& enemy, float deltaTime) const;
    bool isOutsideWorld(const Enemy& enemy) const;

    std::unordered_map<network::NetworkId, Enemy> _enemies;
    network::NetworkId _nextEnemyId{game::FIRST_ENEMY_ID};
    std::mt19937 _rng;
    float _spawnTimer{0.0f};
  };
}

#endif // !ENEMYMANAGER_HPP
