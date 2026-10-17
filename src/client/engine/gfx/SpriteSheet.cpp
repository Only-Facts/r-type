#include "gfx/SpriteSheet.hpp"
#include <algorithm>

namespace rtype::engine {
  SpriteSheet::SpriteSheet(int frameWidth, int frameHeight, int columnCount, int rowCount)
    : _frameWidth(frameWidth),
      _frameHeight(frameHeight),
      _columnCount(std::max(columnCount, 1)),
      _rowCount(std::max(rowCount, 1)) {}

  IntRect SpriteSheet::getFrameRect(int column, int row) const {
    const int safeColumn = clampIndex(column, _columnCount);
    const int safeRow = clampIndex(row, _rowCount);
    return IntRect{safeColumn * _frameWidth, safeRow * _frameHeight, _frameWidth, _frameHeight};
  }

  int SpriteSheet::getColumnCount() const {
    return _columnCount;
  }

  int SpriteSheet::getRowCount() const {
    return _rowCount;
  }

  int SpriteSheet::clampIndex(int index, int count) {
    return std::clamp(index, 0, count - 1);
  }
}
