#ifndef RTYPE_ENGINE_IAUDIOMANAGER_HPP
#define RTYPE_ENGINE_IAUDIOMANAGER_HPP

#include <string>

namespace rtype::engine {

  class IAudioManager {
  public:
    virtual ~IAudioManager() = default;

    virtual void loadSound(const std::string& filepath) = 0;
    virtual void playSound(const std::string& filepath, float volume, float pitch) = 0;

    virtual void playMusic(const std::string& filepath, bool loop, float volume) = 0;
    virtual void setMusicVolume(float volume) = 0;
    virtual void stopMusic() = 0;

    virtual void update() = 0;
  };
}

#endif // !RTYPE_ENGINE_IAUDIOMANAGER_HPP
