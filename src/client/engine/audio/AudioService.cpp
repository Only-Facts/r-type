#include "engine/audio/AudioService.hpp"
#include "engine/config/Config.hpp"
#include "exception.hpp"

namespace rtype::engine {
  namespace {
    float readVolume(const Config& config, const std::string& section, const std::string& key, float fallback) {
      const float value = config.getFloat(section, key, fallback);
      if (value < 0.0f || value > 100.0f) throw RtypeError("[" + section + "] " + key + " must be in [0, 100]");
      return value;
    }
  }

  void AudioService::loadSettings(const Config& config, const std::string& section) {
    _enabled = config.getBool(section, "enabled", _enabled);
    _muted = config.getBool(section, "muted", _muted);
    _master = readVolume(config, section, "master_volume", _master);
    _music = readVolume(config, section, "music_volume", _music);
    _sfx = readVolume(config, section, "sfx_volume", _sfx);
  }

  void AudioService::play(const std::string& soundId) {
    if (!_enabled || _muted) return;
    const AssetDescriptor& desc = _resources.get(soundId, AssetType::Sound);
    const float volume = desc.volume * (_master / 100.0f) * (_sfx / 100.0f);
    _backend.playSound(desc.path, volume, desc.pitch);
  }

  void AudioService::playMusic(const std::string& musicId) {
    if (!_enabled) return;
    const AssetDescriptor& desc = _resources.get(musicId, AssetType::Music);
    _currentMusic = musicId;
    _backend.playMusic(desc.path, desc.loop, musicVolume());
  }

  void AudioService::stopMusic() {
    if (!_enabled) return;
    _currentMusic.clear();
    _backend.stopMusic();
  }

  void AudioService::setMuted(bool muted) {
    _muted = muted;
    if (_enabled && !_currentMusic.empty()) _backend.setMusicVolume(musicVolume());
  }

  float AudioService::musicVolume() const {
    if (_muted || _currentMusic.empty()) return 0.0f;
    const AssetDescriptor& desc = _resources.get(_currentMusic, AssetType::Music);
    return desc.volume * (_master / 100.0f) * (_music / 100.0f);
  }
}
