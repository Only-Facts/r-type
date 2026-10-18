#ifndef RTYPE_ENGINE_RESOURCEMANAGER_HPP
#define RTYPE_ENGINE_RESOURCEMANAGER_HPP

#include "engine/audio/IAudioManager.hpp"
#include "engine/gfx/IFontManager.hpp"
#include "engine/gfx/ISprite.hpp"
#include "engine/gfx/IText.hpp"
#include "engine/gfx/ITextureManager.hpp"
#include "engine/resources/AssetDescriptor.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::engine {
  class Config;

  /// Asset registry: logical id -> descriptor (type, path, rect, scale, tint, volume...).
  /// The game asks for "player_1" or "music_stage1" and never sees a file path
  /// or a backend type.
  class ResourceManager {
  public:
    ResourceManager(ITextureManager& textures, IFontManager& fonts, IAudioManager& audio)
      : _textures(textures), _fonts(fonts), _audio(audio) {}

    /// Each section of the manifest is one asset, the section name being its id.
    void loadManifest(const Config& manifest);
    void loadManifestFile(const std::string& path);
    void registerAsset(AssetDescriptor descriptor);

    bool has(const std::string& id) const;
    /// Throws RtypeError if the id is unknown.
    const AssetDescriptor& get(const std::string& id) const;
    /// Same, and also checks the asset type (a font id used as a sprite is an error).
    const AssetDescriptor& get(const std::string& id, AssetType expected) const;
    std::vector<std::string> getIds() const;

    /// Creates a sprite with the manifest rect, scale and tint already applied.
    std::shared_ptr<ISprite> createSprite(const std::string& id);
    std::shared_ptr<IText> createText(const std::string& fontId);

    /// Loads every texture, font and sound now (loading screen) instead of on first use.
    /// Music is streamed, so it is not preloaded.
    void preloadAll();
    void clear();

  private:
    ITextureManager& _textures;
    IFontManager& _fonts;
    IAudioManager& _audio;
    std::unordered_map<std::string, AssetDescriptor> _assets;
  };
}

#endif // !RTYPE_ENGINE_RESOURCEMANAGER_HPP
