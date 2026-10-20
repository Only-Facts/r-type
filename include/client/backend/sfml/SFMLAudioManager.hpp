#ifndef RTYPE_BACKEND_SFMLAUDIOMANAGER_HPP
#define RTYPE_BACKEND_SFMLAUDIOMANAGER_HPP

#include "engine/audio/IAudioManager.hpp"
#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <cstddef>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

namespace rtype::backend {
  class SFMLAudioManager : public engine::IAudioManager {
  public:
    SFMLAudioManager() = default;
    ~SFMLAudioManager() override;

    void loadSound(const std::string& filepath) override;
    void playSound(const std::string& filepath, float volume, float pitch) override;

    void playMusic(const std::string& filepath, bool loop, float volume) override;
    void setMusicVolume(float volume) override;
    void stopMusic() override;

    void update() override;

  private:
    static constexpr std::size_t MAX_VOICES = 32;

    const sf::SoundBuffer& getBuffer(const std::string& filepath);

    std::unordered_map<std::string, std::unique_ptr<sf::SoundBuffer>> _buffers;
    std::list<sf::Sound> _voices;
    std::unique_ptr<sf::Music> _music;
    std::string _musicPath;
  };
}

#endif // !RTYPE_BACKEND_SFMLAUDIOMANAGER_HPP
