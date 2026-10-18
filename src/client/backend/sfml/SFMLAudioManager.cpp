#include "backend/sfml/SFMLAudioManager.hpp"
#include "exception.hpp"

namespace rtype::backend {
  SFMLAudioManager::~SFMLAudioManager() {    _voices.clear();
    if (_music) _music->stop();
  }

  void SFMLAudioManager::loadSound(const std::string& filepath) {
    getBuffer(filepath);
  }

  void SFMLAudioManager::playSound(const std::string& filepath, float volume, float pitch) {
    const sf::SoundBuffer& buffer = getBuffer(filepath);

    update();
    if (_voices.size() >= MAX_VOICES) _voices.pop_front(); 

    sf::Sound& sound = _voices.emplace_back(buffer);
    sound.setVolume(volume);
    sound.setPitch(pitch);
    sound.play();
  }

  void SFMLAudioManager::playMusic(const std::string& filepath, bool loop, float volume) {
    if (!_music || _musicPath != filepath) {
      auto music = std::make_unique<sf::Music>();
      if (!music->openFromFile(filepath)) throw RtypeError("Failed to open music file: " + filepath);
      if (_music) _music->stop();
      _music = std::move(music);
      _musicPath = filepath;
    }
    _music->setLooping(loop);
    _music->setVolume(volume);
    if (_music->getStatus() != sf::SoundSource::Status::Playing) _music->play();
  }

  void SFMLAudioManager::setMusicVolume(float volume) {
    if (_music) _music->setVolume(volume);
  }

  void SFMLAudioManager::stopMusic() {
    if (_music) _music->stop();
  }

  void SFMLAudioManager::update() {
    _voices.remove_if([](const sf::Sound& sound) {
      return sound.getStatus() == sf::SoundSource::Status::Stopped;
    });
  }

  const sf::SoundBuffer& SFMLAudioManager::getBuffer(const std::string& filepath) {
    auto it = _buffers.find(filepath);
    if (it != _buffers.end()) return *it->second;

    auto buffer = std::make_unique<sf::SoundBuffer>();
    if (!buffer->loadFromFile(filepath)) throw RtypeError("Failed to load sound from file: " + filepath);
    return *_buffers.emplace(filepath, std::move(buffer)).first->second;
  }
}
