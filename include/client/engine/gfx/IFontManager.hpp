#ifndef RTYPE_ENGINE_IFONTMANAGER_HPP
#define RTYPE_ENGINE_IFONTMANAGER_HPP

#include "engine/gfx/IText.hpp"
#include <memory>
#include <string>

namespace rtype::engine {
  /// Low-level font cache, implemented by the backend.
  /// Games go through ResourceManager and font asset ids instead.
  class IFontManager {
  public:
    virtual ~IFontManager() = default;

    /// Loads (and caches) a font. Throws RtypeError on failure.
    virtual void loadFont(const std::string& filepath) = 0;
    virtual std::shared_ptr<IText> createText(const std::string& filepath) = 0;
    virtual void clear() = 0;
  };
}

#endif // !RTYPE_ENGINE_IFONTMANAGER_HPP
