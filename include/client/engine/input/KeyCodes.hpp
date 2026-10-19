#ifndef RTYPE_ENGINE_KEYCODES_HPP
#define RTYPE_ENGINE_KEYCODES_HPP

#include <cstddef>
#include <optional>
#include <string_view>

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
    Tab, Backspace,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Count
  };

  std::optional<KeyCode> keyCodeFromString(std::string_view name);

  std::string_view keyCodeToString(KeyCode key);
}

#endif // !RTYPE_ENGINE_KEYCODES_HPP
