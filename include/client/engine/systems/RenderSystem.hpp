#ifndef RTYPE_ENGINE_RENDERSYSTEM_HPP
#define RTYPE_ENGINE_RENDERSYSTEM_HPP

#include "engine/ecs/Components.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/gfx/ISprite.hpp"
#include "engine/gfx/IText.hpp"
#include "engine/platform/IWindow.hpp"
#include "engine/resources/ResourceManager.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::engine {
  /// Draws every entity with a Transform and a Sprite and/or Text component,
  /// all sorted together by zIndex. It owns the backend objects (one per entity,
  /// created on demand from the asset id and released when the entity disappears),
  /// so components stay pure data.
  class RenderSystem {
  public:
    explicit RenderSystem(ResourceManager& resources) : _resources(resources) {}

    void render(Registry& registry, IWindow& window);
    std::size_t getCachedSpriteCount() const { return _sprites.size(); }
    std::size_t getCachedTextCount() const { return _texts.size(); }

  private:
    struct CachedSprite {
      std::string assetId;
      std::shared_ptr<ISprite> sprite;
      IntRect baseRect{};
      Vector2f baseScale{1.0f, 1.0f};
      Color baseTint{};
      std::uint64_t lastFrame{0};
    };

    struct CachedText {
      std::string fontId;
      std::shared_ptr<IText> text;
      std::string lastString;
      unsigned int lastSize{0};
      std::uint64_t lastFrame{0};
    };

    struct DrawItem {
      Entity entity;
      int zIndex;
      bool isText;
      const void* component;
      const TransformComponent* transform;
    };

    void drawSprite(Registry& registry, IWindow& window, const DrawItem& item);
    void drawText(IWindow& window, const DrawItem& item);
    CachedSprite& resolveSprite(Entity entity, const std::string& assetId);
    CachedText& resolveText(Entity entity, const std::string& fontId);

    template<typename Cache>
    void prune(Cache& cache) {
      for (auto it = cache.begin(); it != cache.end();) {
        if (it->second.lastFrame != _frame) it = cache.erase(it);
        else ++it;
      }
    }

    ResourceManager& _resources;
    std::unordered_map<Entity, CachedSprite> _sprites;
    std::unordered_map<Entity, CachedText> _texts;
    std::vector<DrawItem> _drawList;
    std::uint64_t _frame{0};
  };
}

#endif // !RTYPE_ENGINE_RENDERSYSTEM_HPP
