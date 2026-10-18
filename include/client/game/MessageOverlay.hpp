#ifndef MESSAGEOVERLAY_HPP
#define MESSAGEOVERLAY_HPP

#include "IWindow.hpp"
#include "gfx/ITextFactory.hpp"
#include <memory>
#include <string>

namespace rtype::client {
  class MessageOverlay {
  public:
    explicit MessageOverlay(engine::ITextFactory& textFactory);

    void setMessage(const std::string& title, const std::string& subtitle, const std::string& hint);
    void render(engine::IWindow& window) const;

  private:
    static void centerHorizontally(engine::IText& text, float windowWidth, float y);

    std::shared_ptr<engine::IText> _title;
    std::shared_ptr<engine::IText> _subtitle;
    std::shared_ptr<engine::IText> _hint;
  };
}

#endif // !MESSAGEOVERLAY_HPP
