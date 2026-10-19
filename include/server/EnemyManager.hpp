#ifndef ENEMYMANAGER_HPP
#define ENEMYMANAGER_HPP

#include "GameConstants.hpp"
#include "network/Protocol.hpp"
#include <cstdint>
#include <random>
#include <unordered_map>

namespace rtype::server {
  struct Enemy {
    network::NetworkId id;
    game::EnemyType type;
    float posX;
    float posY;
    int health;
  };

  class EnemyManager {
  public:
    EnemyManager();

    void update(float deltaTime);

    bool applyDamage(network::NetworkId enemyId, int damage);
    void remove(network::NetworkId enemyId);

    const std::unordered_map<network::NetworkId, Enemy>& getEnemies() const;

    static game::Hitbox getHitbox(const Enemy& enemy);
    static std::uint32_t getScoreValue(game::EnemyType type);
    static float getExplosionScale(game::EnemyType type);

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
