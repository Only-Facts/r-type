#include "EnemyManager.hpp"
#include <array>
#include <cmath>
#include <vector>

namespace rtype::server {
  namespace {
    struct EnemyStats {
      game::Hitbox hitbox;
      float speed;
      float waveAmplitude;
      float waveFrequency;
      int maxHealth;
      std::uint32_t scoreValue;
      float explosionScale;
    };

    constexpr std::array<EnemyStats, static_cast<std::size_t>(game::EnemyType::Count)> ENEMY_STATS{{
      {{3.0f, 3.0f, 42.0f, 39.0f}, 320.0f, 90.0f, 3.0f, 1, 100, 3.0f},
      {{6.0f, 6.0f, 78.0f, 66.0f}, 200.0f, 0.0f, 0.0f, 2, 200, 5.0f},
      {{6.0f, 6.0f, 84.0f, 81.0f}, 120.0f, 40.0f, 1.2f, 4, 400, 6.0f},
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

  bool EnemyManager::applyDamage(network::NetworkId enemyId, int damage) {
    auto it = _enemies.find(enemyId);
    if (it == _enemies.end()) return false;

    it->second.health -= damage;
    if (it->second.health > 0) return false;

    _enemies.erase(it);
    return true;
  }

  void EnemyManager::remove(network::NetworkId enemyId) {
    _enemies.erase(enemyId);
  }

  const std::unordered_map<network::NetworkId, Enemy>& EnemyManager::getEnemies() const {
    return _enemies;
  }

  game::Hitbox EnemyManager::getHitbox(const Enemy& enemy) {
    return getStats(enemy.type).hitbox.movedTo(enemy.posX, enemy.posY);
  }

  std::uint32_t EnemyManager::getScoreValue(game::EnemyType type) {
    return getStats(type).scoreValue;
  }

  float EnemyManager::getExplosionScale(game::EnemyType type) {
    return getStats(type).explosionScale;
  }

  void EnemyManager::spawnRandomEnemy() {
    std::uniform_int_distribution<int> typeDistribution(0, static_cast<int>(game::EnemyType::Count) - 1);
    const auto type = static_cast<game::EnemyType>(typeDistribution(_rng));
    const EnemyStats& stats = getStats(type);

    const float spriteHeight = stats.hitbox.height + 2.0f * stats.hitbox.y;
    const float minY = SCREEN_EDGE_MARGIN + stats.waveAmplitude;
    const float maxY = game::WORLD_HEIGHT - SCREEN_EDGE_MARGIN - stats.waveAmplitude - spriteHeight;
    std::uniform_real_distribution<float> yDistribution(minY, maxY);
    const float spawnY = yDistribution(_rng);

    const Enemy enemy{_nextEnemyId++, type, game::WORLD_WIDTH, spawnY, spawnY, 0.0f, stats.maxHealth};
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
    const game::Hitbox& hitbox = getStats(enemy.type).hitbox;
    return enemy.posX < -(game::WORLD_DESPAWN_MARGIN + hitbox.x + hitbox.width);
  }
}
