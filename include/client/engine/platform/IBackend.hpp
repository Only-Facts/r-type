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
  class IBackend {
  public:
    virtual ~IBackend() = default;

    virtual IWindow& getWindow() = 0;
    virtual IInputManager& getInput() = 0;
    virtual ITextureManager& getTextures() = 0;
    virtual IFontManager& getFonts() = 0;
    virtual IAudioManager& getAudio() = 0;

    virtual void pollEvents() = 0;

    virtual std::unique_ptr<network::INetworkSocket> createUdpSocket() = 0;
  };
}

#endif // !RTYPE_ENGINE_IBACKEND_HPP
