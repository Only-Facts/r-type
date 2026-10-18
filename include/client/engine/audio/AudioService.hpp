#ifndef RTYPE_ENGINE_AUDIOSERVICE_HPP
#define RTYPE_ENGINE_AUDIOSERVICE_HPP

#include "engine/audio/IAudioManager.hpp"
#include "engine/resources/ResourceManager.hpp"
#include <string>

namespace rtype::engine {
  class Config;

  /// Id-based audio for games: play("explosion"), playMusic("music_stage1").
  /// Applies the [audio] settings (enabled, master/music/sfx volumes, mute),
  /// which are also an accessibility feature (players can lower or cut sound).
  class AudioService {
  public:
    AudioService(IAudioManager& backend, const ResourceManager& resources)
      : _backend(backend), _resources(resources) {}

    void loadSettings(const Config& config, const std::string& section);

    void play(const std::string& soundId);
    void playMusic(const std::string& musicId);
    void stopMusic();

    void setMuted(bool muted);
    void toggleMute() { setMuted(!_muted); }
    bool isMuted() const { return _muted; }
    bool isEnabled() const { return _enabled; }

    void update() { if (_enabled) _backend.update(); }

  private:
    float musicVolume() const;

    IAudioManager& _backend;
    const ResourceManager& _resources;
    bool _enabled{true};
    bool _muted{false};
    float _master{100.0f};
    float _music{60.0f};
    float _sfx{80.0f};
    std::string _currentMusic;
  };
}

#endif // !RTYPE_ENGINE_AUDIOSERVICE_HPP
