#ifndef RTYPE_ENGINE_ASSETDESCRIPTOR_HPP
#define RTYPE_ENGINE_ASSETDESCRIPTOR_HPP

#include "engine/gfx/GfxTypes.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace rtype::engine {
  enum class AssetType { Texture, Font, Sound, Music };

  std::string_view assetTypeToString(AssetType type);

  /// Everything the engine needs to know about an asset, read from the
  /// asset manifest. The game only knows the `id`.
  struct AssetDescriptor {
    std::string id;
    AssetType type{AssetType::Texture};
    std::string path;

    // Texture
    std::optional<IntRect> rect;   ///< First frame in a spritesheet (whole texture if empty).
    Vector2f scale{1.0f, 1.0f};
    Color tint{255, 255, 255, 255};
    int frames{1};                 ///< Frames laid out horizontally, starting at `rect`.
    float frameTime{0.1f};
    bool loop{true};               ///< Animation loop, or music loop.

    // Sound / Music
    float volume{100.0f};          ///< [0, 100], multiplied by the [audio] volumes.
    float pitch{1.0f};
  };
}

#endif // !RTYPE_ENGINE_ASSETDESCRIPTOR_HPP
