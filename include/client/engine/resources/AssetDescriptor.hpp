#ifndef RTYPE_ENGINE_ASSETDESCRIPTOR_HPP
#define RTYPE_ENGINE_ASSETDESCRIPTOR_HPP

#include "engine/gfx/GfxTypes.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace rtype::engine {
  enum class AssetType { Texture, Font, Sound, Music };

  std::string_view assetTypeToString(AssetType type);


  struct AssetDescriptor {
    std::string id;
    AssetType type{AssetType::Texture};
    std::string path;

    std::optional<IntRect> rect; 
    Vector2f scale{1.0f, 1.0f};
    Color tint{255, 255, 255, 255};
    int frames{1};               
    float frameTime{0.1f};
    bool loop{true};        

    float volume{100.0f};       
    float pitch{1.0f};
  };
}

#endif // !RTYPE_ENGINE_ASSETDESCRIPTOR_HPP
