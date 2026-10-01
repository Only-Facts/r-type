#ifndef KEYCODES_HPP
#define KEYCODES_HPP

#include <cstddef>

namespace rtype::engine {
  enum class KeyCode : std::size_t {
    Unknown = 0,
    Up,
    Down,
    Left,
    Right,
    Space,
    Escape,
    Enter,
    KeyA, KeyB, KeyC, KeyD, KeyE, KeyF, KeyG, KeyH, KeyI, KeyJ,
    KeyK, KeyL, KeyM, KeyN, KeyO, KeyP, KeyQ, KeyR, KeyS, KeyT,
    KeyU, KeyV, KeyW, KeyX, KeyY, KeyZ,
    Count
  };
}

#endif // !KEYCODES_HPP
