#include "engine/resources/ResourceManager.hpp"
#include "engine/config/Config.hpp"
#include "exception.hpp"

namespace rtype::engine {
  std::string_view assetTypeToString(AssetType type) {
    switch (type) {
      case AssetType::Texture: return "texture";
      case AssetType::Font: return "font";
      case AssetType::Sound: return "sound";
      case AssetType::Music: return "music";
    }
    return "unknown";
  }

  namespace {
    AssetType parseType(const Config& manifest, const std::string& id) {
      const std::string type = manifest.getString(id, "type", "texture");
      if (type == "texture") return AssetType::Texture;
      if (type == "font") return AssetType::Font;
      if (type == "sound") return AssetType::Sound;
      if (type == "music") return AssetType::Music;
      throw RtypeError(manifest.getOrigin() + ": asset [" + id + "] unknown type '" + type +
                       "' (texture, font, sound, music)");
    }
  }

  void ResourceManager::loadManifest(const Config& manifest) {
    for (const auto& id : manifest.getSections()) {
      const std::string where = manifest.getOrigin() + ": asset [" + id + "] ";
      AssetDescriptor desc;
      desc.id = id;
      desc.type = parseType(manifest, id);
      desc.path = manifest.getString(id, "path");
      if (desc.path.empty()) throw RtypeError(where + "has no 'path'");

      if (manifest.has(id, "rect")) {
        const auto r = manifest.getIntList(id, "rect");
        if (r.size() != 4 || r[2] <= 0 || r[3] <= 0)
          throw RtypeError(where + "rect must be 'left top width height'");
        desc.rect = IntRect{r[0], r[1], r[2], r[3]};
      }

      if (manifest.has(id, "scale")) {
        const auto s = manifest.getFloatList(id, "scale");
        if (s.size() == 1) desc.scale = {s[0], s[0]};
        else if (s.size() == 2) desc.scale = {s[0], s[1]};
        else throw RtypeError(where + "scale must be 's' or 'sx sy'");
      }

      desc.tint = manifest.getColor(id, "tint", desc.tint);
      desc.frames = manifest.getInt(id, "frames", desc.frames);
      desc.frameTime = manifest.getFloat(id, "frame_time", desc.frameTime);
      desc.loop = manifest.getBool(id, "loop", desc.loop);
      desc.volume = manifest.getFloat(id, "volume", desc.volume);
      desc.pitch = manifest.getFloat(id, "pitch", desc.pitch);

      if (desc.frames < 1) throw RtypeError(where + "frames must be >= 1");
      if (desc.volume < 0.0f || desc.volume > 100.0f) throw RtypeError(where + "volume must be in [0, 100]");
      if (desc.pitch <= 0.0f) throw RtypeError(where + "pitch must be > 0");

      registerAsset(std::move(desc));
    }
  }

  void ResourceManager::loadManifestFile(const std::string& path) {
    loadManifest(Config::fromFile(path));
  }

  void ResourceManager::registerAsset(AssetDescriptor descriptor) {
    std::string id = descriptor.id;
    _assets[id] = std::move(descriptor);
  }

  bool ResourceManager::has(const std::string& id) const {
    return _assets.find(id) != _assets.end();
  }

  const AssetDescriptor& ResourceManager::get(const std::string& id) const {
    auto it = _assets.find(id);
    if (it == _assets.end()) throw RtypeError("Unknown asset id: '" + id + "' (missing from the asset manifest)");
    return it->second;
  }

  const AssetDescriptor& ResourceManager::get(const std::string& id, AssetType expected) const {
    const AssetDescriptor& desc = get(id);
    if (desc.type != expected)
      throw RtypeError("Asset '" + id + "' is a " + std::string(assetTypeToString(desc.type)) +
                       ", expected a " + std::string(assetTypeToString(expected)));
    return desc;
  }

  std::vector<std::string> ResourceManager::getIds() const {
    std::vector<std::string> ids;
    ids.reserve(_assets.size());
    for (const auto& [id, desc] : _assets) ids.push_back(id);
    return ids;
  }

  std::shared_ptr<ISprite> ResourceManager::createSprite(const std::string& id) {
    const AssetDescriptor& desc = get(id, AssetType::Texture);
    auto sprite = _textures.createSprite(desc.path);
    if (desc.rect) sprite->setTextureRect(*desc.rect);
    sprite->setScale(desc.scale);
    sprite->setColor(desc.tint);
    return sprite;
  }

  std::shared_ptr<IText> ResourceManager::createText(const std::string& fontId) {
    return _fonts.createText(get(fontId, AssetType::Font).path);
  }

  void ResourceManager::preloadAll() {
    for (const auto& [id, desc] : _assets) {
      switch (desc.type) {
        case AssetType::Texture: _textures.loadTexture(desc.path); break;
        case AssetType::Font: _fonts.loadFont(desc.path); break;
        case AssetType::Sound: _audio.loadSound(desc.path); break;
        case AssetType::Music: break;
      }
    }
  }

  void ResourceManager::clear() {
    _assets.clear();
    _textures.clear();
    _fonts.clear();
  }
}
