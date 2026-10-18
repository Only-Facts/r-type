#ifndef ITEXTFACTORY_HPP
#define ITEXTFACTORY_HPP

#include "IText.hpp"
#include <memory>

namespace rtype::engine {
  class ITextFactory {
  public:
    virtual ~ITextFactory() = default;

    virtual std::shared_ptr<IText> createText(unsigned int characterSize) = 0;
  };
}

#endif // !ITEXTFACTORY_HPP
