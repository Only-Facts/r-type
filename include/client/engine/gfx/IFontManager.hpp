#ifndef RTYPE_ENGINE_IFONTMANAGER_HPP
#define RTYPE_ENGINE_IFONTMANAGER_HPP

#include "engine/gfx/IText.hpp"
#include <memory>
#include <string>

namespace rtype::engine {

  class IFontManager {
  public:
    virtual ~IFontManager() = default;

    virtual void loadFont(const std::string& filepath) = 0;
    virtual std::shared_ptr<IText> createText(const std::string& filepath) = 0;
    virtual void clear() = 0;
  };
}

#endif // !RTYPE_ENGINE_IFONTMANAGER_HPP
