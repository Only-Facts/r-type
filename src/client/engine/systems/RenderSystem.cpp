#include "engine/systems/RenderSystem.hpp"
#include <algorithm>

namespace rtype::engine {
  void RenderSystem::render(Registry& registry, IWindow& window) {
    ++_frame;
    _drawList.clear();

    registry.each<SpriteComponent, TransformComponent>(
      [this](Entity entity, SpriteComponent& sprite, TransformComponent& transform) {
        if (sprite.visible) _drawList.push_back({entity, sprite.zIndex, false, &sprite, &transform});
      });
    registry.each<TextComponent, TransformComponent>(
      [this](Entity entity, TextComponent& text, TransformComponent& transform) {
        if (text.visible && !text.text.empty()) _drawList.push_back({entity, text.zIndex, true, &text, &transform});
      });

    std::sort(_drawList.begin(), _drawList.end(), [](const DrawItem& a, const DrawItem& b) {
      if (a.zIndex != b.zIndex) return a.zIndex < b.zIndex;
      if (a.isText != b.isText) return !a.isText;  
      return a.entity < b.entity;
    });

    for (const auto& item : _drawList) {
      if (item.isText) drawText(window, item);
      else drawSprite(registry, window, item);
    }

    prune(_sprites);
    prune(_texts);
  }

  void RenderSystem::drawSprite(Registry& registry, IWindow& window, const DrawItem& item) {
    const auto& component = *static_cast<const SpriteComponent*>(item.component);
    const TransformComponent& transform = *item.transform;
    CachedSprite& cached = resolveSprite(item.entity, component.assetId);
    ISprite& sprite = *cached.sprite;

    sprite.setPosition(transform.position);
    sprite.setRotation(transform.rotation);
    sprite.setScale({cached.baseScale.x * transform.scale.x, cached.baseScale.y * transform.scale.y});
    sprite.setColor(component.tint.value_or(cached.baseTint));

    IntRect rect = cached.baseRect;
    if (const auto* anim = registry.getComponent<AnimationComponent>(item.entity))
      rect.left += anim->currentFrame * rect.width;
    sprite.setTextureRect(rect);

    window.draw(sprite);
    cached.lastFrame = _frame;
  }

  void RenderSystem::drawText(IWindow& window, const DrawItem& item) {
    const auto& component = *static_cast<const TextComponent*>(item.component);
    CachedText& cached = resolveText(item.entity, component.fontId);
    IText& text = *cached.text;

    if (cached.lastString != component.text) {
      text.setString(component.text);
      cached.lastString = component.text;
    }
    if (cached.lastSize != component.characterSize) {
      text.setCharacterSize(component.characterSize);
      cached.lastSize = component.characterSize;
    }
    text.setColor(component.color);
    text.setOutline(component.outlineColor, component.outlineThickness);

    const Vector2f size = text.getSize();
    const Vector2f& pos = item.transform->position;
    text.setPosition({pos.x - component.origin.x * size.x, pos.y - component.origin.y * size.y});

    window.draw(text);
    cached.lastFrame = _frame;
  }

  RenderSystem::CachedSprite& RenderSystem::resolveSprite(Entity entity, const std::string& assetId) {
    auto it = _sprites.find(entity);
    if (it != _sprites.end() && it->second.assetId == assetId) return it->second;

    const AssetDescriptor& desc = _resources.get(assetId, AssetType::Texture);
    CachedSprite cached;
    cached.assetId = assetId;
    cached.sprite = _resources.createSprite(assetId);
    cached.baseRect = cached.sprite->getTextureRect();
    cached.baseScale = desc.scale;
    cached.baseTint = desc.tint;

    return _sprites.insert_or_assign(entity, std::move(cached)).first->second;
  }

  RenderSystem::CachedText& RenderSystem::resolveText(Entity entity, const std::string& fontId) {
    auto it = _texts.find(entity);
    if (it != _texts.end() && it->second.fontId == fontId) return it->second;

    CachedText cached;
    cached.fontId = fontId;
    cached.text = _resources.createText(fontId);

    return _texts.insert_or_assign(entity, std::move(cached)).first->second;
  }
}
