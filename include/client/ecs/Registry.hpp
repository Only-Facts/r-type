#ifndef REGISTRY_HPP
#define REGISTRY_HPP

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <typeindex>

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
    bool hasComponent(Entity entity) const {
      return getPool<T>()->has(entity);
    }

    template<typename T>
    void removeComponent(Entity entity) {
      getPool<T>()->remove(entity);
    }

    template<typename T, typename Func>
    void view(Func&& func) {
      auto& data = getPool<T>()->getData();
      for (auto& [entity, component] : data)
        func(entity, component);
    }

  private:
    Entity _nextEntityId{1};
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> _componentPools;

    template<typename T>
    ComponentPool<T>* getPool() {
      std::type_index type = typeid(T);
      if (_componentPools.find(type) == _componentPools.end())
        _componentPools[type] = std::make_unique<ComponentPool<T>>();
      return static_cast<ComponentPool<T>*>(_componentPools[type].get());
    }
  };
}

#endif // !REGISTRY_HPP
