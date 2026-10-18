#ifndef RTYPE_ENGINE_IAUDIOMANAGER_HPP
#define RTYPE_ENGINE_IAUDIOMANAGER_HPP

#include <string>

namespace rtype::engine {
  /// Low-level audio playback, implemented by the backend.
  /// Games go through AudioService and sound asset ids instead.
  /// Volumes are in [0, 100].
  class IAudioManager {
  public:
    virtual ~IAudioManager() = default;

    /// Loads (and caches) a sound effect. Throws RtypeError on failure.
    virtual void loadSound(const std::string& filepath) = 0;
    virtual void playSound(const std::string& filepath, float volume, float pitch) = 0;

    /// Music is streamed from disk, one track at a time.
    virtual void playMusic(const std::string& filepath, bool loop, float volume) = 0;
    virtual void setMusicVolume(float volume) = 0;
    virtual void stopMusic() = 0;

    /// Called once per frame by the engine (releases finished sounds).
    virtual void update() = 0;
  };
}

#endif // !RTYPE_ENGINE_IAUDIOMANAGER_HPP
