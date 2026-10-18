#ifndef RTYPE_ENGINE_ASCENE_HPP
#define RTYPE_ENGINE_ASCENE_HPP

#include "engine/core/EngineContext.hpp"
#include "engine/ecs/EntityFactory.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/ecs/SystemPipeline.hpp"
#include "engine/scene/IScene.hpp"
#include "engine/systems/RenderSystem.hpp"
#include "engine/tools/DebugOverlay.hpp"
#include <utility>

namespace rtype::engine {
  /// Base class for ECS scenes: owns a Registry, a system pipeline and a renderer.
  /// A concrete scene only declares which systems it needs and which entities it
  /// spawns (onInit), plus its per-frame logic (onUpdate).
  ///
  /// Every scene also gets, for free, after its own systems:
  ///   ScreenAnchorSystem, BlinkSystem, FlashSystem, LifetimeSystem and the DebugOverlay.
  class AScene : public IScene {
  public:
    explicit AScene(EngineContext& context);
    ~AScene() override = default;

    AScene(const AScene&) = delete;
    AScene& operator=(const AScene&) = delete;

    void init() final;
    void update(float dt) override;
    void render(IWindow& window) override;

  protected:
    /// Called once, the first time the scene becomes active.
    virtual void onInit() = 0;
    /// Called every frame, before the systems run.
    virtual void onUpdate(float dt);

    template<typename T, typename... Args>
    T& addSystem(Args&&... args) {
      return _systems.add<T>(std::forward<Args>(args)...);
    }

    EngineContext& _context;
    Registry _registry;
    SystemPipeline _systems;
    EntityFactory _factory;
    RenderSystem _renderer;

  private:
    SystemPipeline _builtinSystems;
    DebugOverlay _debugOverlay;
    bool _initialized{false};
  };
}

#endif // !RTYPE_ENGINE_ASCENE_HPP
