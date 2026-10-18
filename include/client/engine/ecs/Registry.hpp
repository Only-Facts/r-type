#ifndef RTYPE_ENGINE_REGISTRY_HPP
#define RTYPE_ENGINE_REGISTRY_HPP

#include <cstdint>
#include <memory>
#include <tuple>
#include <typeindex>
#include <unordered_map>

namespace rtype::engine {
  using Entity = std::uint32_t;

  class IComponentPool {
  public:
    virtual ~IComponentPool() = default;
    virtual void entityDestroyed(Entity entity) = 0;
  };

  template<typename T>
  class ComponentPool : public IComponentPool {
  public:
    void insert(Entity entity, T component) {
      _data[entity] = std::move(component);
    }

    void remove(Entity entity) {
      _data.erase(entity);
    }

    T* get(Entity entity) {
      auto it = _data.find(entity);
      return (it != _data.end()) ? &it->second : nullptr;
    }

    bool has(Entity entity) const {
      return _data.find(entity) != _data.end();
    }

    void entityDestroyed(Entity entity) override {
      _data.erase(entity);
    }

    std::unordered_map<Entity, T>& getData() {
      return _data;
    }

  private:
    std::unordered_map<Entity, T> _data;
  };

  class Registry {
  public:
    Registry() = default;
    ~Registry() = default;

    Entity createEntity() {
      return _nextEntityId++;
    }

    void destroyEntity(Entity entity) {
      for (auto& [type, pool] : _componentPools)
        pool->entityDestroyed(entity);
    }

    template<typename T>
    T& addComponent(Entity entity, T component) {
      getPool<T>()->insert(entity, std::move(component));
      return *getComponent<T>(entity);
    }

    template<typename T>
    T* getComponent(Entity entity) {
      return getPool<T>()->get(entity);
    }

    template<typename T>
    bool hasComponent(Entity entity) {
      return getPool<T>()->has(entity);
    }

    template<typename T>
    void removeComponent(Entity entity) {
      getPool<T>()->remove(entity);
    }

    /// Iterates every entity owning a T: func(Entity, T&).
    template<typename T, typename Func>
    void view(Func&& func) {
      auto& data = getPool<T>()->getData();
      for (auto& [entity, component] : data)
        func(entity, component);
    }

    /// Iterates every entity owning ALL the listed components:
    /// func(Entity, T&, Others&...). Do not add/remove T components inside func.
    template<typename T, typename... Others, typename Func>
    void each(Func&& func) {
      auto& data = getPool<T>()->getData();
      for (auto& [entity, component] : data) {
        if constexpr (sizeof...(Others) == 0) {
          func(entity, component);
        } else {
          auto others = std::make_tuple(getComponent<Others>(entity)...);
          if ((std::get<Others*>(others) && ...))
            func(entity, component, *std::get<Others*>(others)...);
        }
      }
    }

  private:
    Entity _nextEntityId{1};
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> _componentPools;

    template<typename T>
    ComponentPool<T>* getPool() {
      std::type_index type = typeid(T);
      auto it = _componentPools.find(type);
      if (it == _componentPools.end())
        it = _componentPools.emplace(type, std::make_unique<ComponentPool<T>>()).first;
      return static_cast<ComponentPool<T>*>(it->second.get());
    }
  };
}

#endif // !RTYPE_ENGINE_REGISTRY_HPP
