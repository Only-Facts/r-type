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

  class ResourceManager {
  public:
    ResourceManager(ITextureManager& textures, IFontManager& fonts, IAudioManager& audio)
      : _textures(textures), _fonts(fonts), _audio(audio) {}

    void loadManifest(const Config& manifest);
    void loadManifestFile(const std::string& path);
    void registerAsset(AssetDescriptor descriptor);

    bool has(const std::string& id) const;
    const AssetDescriptor& get(const std::string& id) const;
    const AssetDescriptor& get(const std::string& id, AssetType expected) const;
    std::vector<std::string> getIds() const;

    std::shared_ptr<ISprite> createSprite(const std::string& id);
    std::shared_ptr<IText> createText(const std::string& fontId);


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
