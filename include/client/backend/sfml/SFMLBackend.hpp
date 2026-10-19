#ifndef RTYPE_BACKEND_SFMLBACKEND_HPP
#define RTYPE_BACKEND_SFMLBACKEND_HPP

#include "backend/sfml/SFMLAudioManager.hpp"
#include "backend/sfml/SFMLFontManager.hpp"
#include "backend/sfml/SFMLInputManager.hpp"
#include "backend/sfml/SFMLTextureManager.hpp"
#include "backend/sfml/SFMLWindow.hpp"
#include "engine/platform/IBackend.hpp"
#include "engine/platform/WindowConfig.hpp"

namespace rtype::backend {
  class SFMLBackend : public engine::IBackend {
  public:
    explicit SFMLBackend(const engine::WindowConfig& config);
    ~SFMLBackend() override = default;

    engine::IWindow& getWindow() override { return _window; }
    engine::IInputManager& getInput() override { return _input; }
    engine::ITextureManager& getTextures() override { return _textures; }
    engine::IFontManager& getFonts() override { return _fonts; }
    engine::IAudioManager& getAudio() override { return _audio; }

    void pollEvents() override;
    std::unique_ptr<network::INetworkSocket> createUdpSocket() override;

  private:
    SFMLWindow _window;
    SFMLInputManager _input;
    SFMLTextureManager _textures;
    SFMLFontManager _fonts;
    SFMLAudioManager _audio;
  };
}

#endif // !RTYPE_BACKEND_SFMLBACKEND_HPP
