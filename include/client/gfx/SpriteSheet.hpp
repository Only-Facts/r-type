#ifndef SPRITESHEET_HPP
#define SPRITESHEET_HPP

#include "GfxTypes.hpp"

namespace rtype::engine {
  // A texture split into a regular grid of equally sized frames.
  // getFrameRect() gives the rectangle to pass to ISprite::setTextureRect().
  class SpriteSheet {
  public:
    SpriteSheet(int frameWidth, int frameHeight, int columnCount, int rowCount);

    // Out-of-range indexes are clamped to the nearest valid frame.
    IntRect getFrameRect(int column, int row) const;

    int getColumnCount() const;
    int getRowCount() const;

  private:
    static int clampIndex(int index, int count);

    int _frameWidth;
    int _frameHeight;
    int _columnCount;
    int _rowCount;
  };
}

#endif // !SPRITESHEET_HPP
