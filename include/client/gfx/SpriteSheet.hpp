#ifndef SPRITESHEET_HPP
#define SPRITESHEET_HPP

#include "GfxTypes.hpp"

namespace rtype::engine {
  class SpriteSheet {
  public:
    SpriteSheet(int frameWidth, int frameHeight, int columnCount, int rowCount);

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
