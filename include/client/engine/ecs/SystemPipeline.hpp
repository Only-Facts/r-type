#ifndef RTYPE_ENGINE_SYSTEMPIPELINE_HPP
#define RTYPE_ENGINE_SYSTEMPIPELINE_HPP

#include "engine/ecs/ISystem.hpp"
#include <memory>
#include <utility>
#include <vector>

namespace rtype::engine {
  /// Ordered list of systems, run once per frame in insertion order.
  class SystemPipeline {
  public:
    template<typename T, typename... Args>
    T& add(Args&&... args) {
      auto system = std::make_unique<T>(std::forward<Args>(args)...);
      T& ref = *system;
      _systems.push_back(std::move(system));
      return ref;
    }

    void update(Registry& registry, float dt) {
      for (auto& system : _systems)
        system->update(registry, dt);
    }

    void clear() { _systems.clear(); }
    std::size_t size() const { return _systems.size(); }

  private:
    std::vector<std::unique_ptr<ISystem>> _systems;
  };
}

#endif // !RTYPE_ENGINE_SYSTEMPIPELINE_HPP
