#ifndef RTYPE_ENGINE_IBACKEND_HPP
#define RTYPE_ENGINE_IBACKEND_HPP

#include "engine/audio/IAudioManager.hpp"
#include "engine/gfx/IFontManager.hpp"
#include "engine/gfx/ITextureManager.hpp"
#include "engine/input/IInputManager.hpp"
#include "engine/platform/IWindow.hpp"
#include "network/INetworkSocket.hpp"
#include <memory>

namespace rtype::engine {
  /// A backend groups every platform-specific service (window, input, textures,
  /// fonts, audio, sockets). It is the only part of the client allowed to know the underlying
  /// library. Swapping SFML for SDL or Raylib means writing a new IBackend,
  /// nothing else changes.
  class IBackend {
  public:
    virtual ~IBackend() = default;

    virtual IWindow& getWindow() = 0;
    virtual IInputManager& getInput() = 0;
    virtual ITextureManager& getTextures() = 0;
    virtual IFontManager& getFonts() = 0;
    virtual IAudioManager& getAudio() = 0;

    /// Pumps OS events into the window and the input manager.
    virtual void pollEvents() = 0;

    /// Creates a UDP socket implemented with the backend's network module.
    virtual std::unique_ptr<network::INetworkSocket> createUdpSocket() = 0;
  };
}

#endif // !RTYPE_ENGINE_IBACKEND_HPP
