#include "EnemyManager.hpp"
#include <array>
#include <cmath>
#include <vector>

namespace rtype::server {
  namespace {
    struct EnemyStats {
      float width;
      float height;
      float speed;
      float waveAmplitude;
      float waveFrequency;
    };

    constexpr std::array<EnemyStats, static_cast<std::size_t>(game::EnemyType::Count)> ENEMY_STATS{{
      {48.0f, 45.0f, 320.0f, 90.0f, 3.0f},
      {90.0f, 78.0f, 200.0f, 0.0f, 0.0f},
      {96.0f, 93.0f, 120.0f, 40.0f, 1.2f},
    }};

    constexpr float SCREEN_EDGE_MARGIN = 20.0f;

    const EnemyStats& getStats(game::EnemyType type) {
      return ENEMY_STATS[static_cast<std::size_t>(type)];
    }
  }

  EnemyManager::EnemyManager() : _rng(std::random_device{}()) {
    scheduleNextSpawn();
  }

  void EnemyManager::update(float deltaTime) {
    _spawnTimer -= deltaTime;
    if (_spawnTimer <= 0.0f) {
      if (_enemies.size() < game::MAX_ENEMIES) spawnRandomEnemy();
      scheduleNextSpawn();
    }

    std::vector<network::NetworkId> expiredIds;
    for (auto& [id, enemy] : _enemies) {
      moveEnemy(enemy, deltaTime);
      if (isOutsideWorld(enemy)) expiredIds.push_back(id);
    }

    for (network::NetworkId id : expiredIds)
      _enemies.erase(id);
  }

  const std::unordered_map<network::NetworkId, Enemy>& EnemyManager::getEnemies() const {
    return _enemies;
  }

  void EnemyManager::spawnRandomEnemy() {
    std::uniform_int_distribution<int> typeDistribution(0, static_cast<int>(game::EnemyType::Count) - 1);
    const auto type = static_cast<game::EnemyType>(typeDistribution(_rng));
    const EnemyStats& stats = getStats(type);

    const float minY = SCREEN_EDGE_MARGIN + stats.waveAmplitude;
    const float maxY = game::WORLD_HEIGHT - SCREEN_EDGE_MARGIN - stats.waveAmplitude - stats.height;
    std::uniform_real_distribution<float> yDistribution(minY, maxY);
    const float spawnY = yDistribution(_rng);

    const Enemy enemy{_nextEnemyId++, type, game::WORLD_WIDTH, spawnY, spawnY, 0.0f};
    _enemies[enemy.id] = enemy;
  }

  void EnemyManager::scheduleNextSpawn() {
    std::uniform_real_distribution<float> delayDistribution(game::ENEMY_SPAWN_MIN_DELAY, game::ENEMY_SPAWN_MAX_DELAY);
    _spawnTimer = delayDistribution(_rng);
  }

  void EnemyManager::moveEnemy(Enemy& enemy, float deltaTime) const {
    const EnemyStats& stats = getStats(enemy.type);

    enemy.age += deltaTime;
    enemy.posX -= stats.speed * deltaTime;
    enemy.posY = enemy.baseY + stats.waveAmplitude * std::sin(enemy.age * stats.waveFrequency);
  }

  bool EnemyManager::isOutsideWorld(const Enemy& enemy) const {
    return enemy.posX < -(game::WORLD_DESPAWN_MARGIN + getStats(enemy.type).width);
  }
}
