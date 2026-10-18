#include "engine/input/KeyCodes.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace rtype::engine {
  namespace {
    struct KeyName {
      KeyCode code;
      std::string_view name;
    };

    constexpr std::array<KeyName, 47> KEY_NAMES{{
      {KeyCode::Up, "up"}, {KeyCode::Down, "down"}, {KeyCode::Left, "left"}, {KeyCode::Right, "right"},
      {KeyCode::Space, "space"}, {KeyCode::Escape, "escape"}, {KeyCode::Enter, "enter"},
      {KeyCode::KeyA, "a"}, {KeyCode::KeyB, "b"}, {KeyCode::KeyC, "c"}, {KeyCode::KeyD, "d"},
      {KeyCode::KeyE, "e"}, {KeyCode::KeyF, "f"}, {KeyCode::KeyG, "g"}, {KeyCode::KeyH, "h"},
      {KeyCode::KeyI, "i"}, {KeyCode::KeyJ, "j"}, {KeyCode::KeyK, "k"}, {KeyCode::KeyL, "l"},
      {KeyCode::KeyM, "m"}, {KeyCode::KeyN, "n"}, {KeyCode::KeyO, "o"}, {KeyCode::KeyP, "p"},
      {KeyCode::KeyQ, "q"}, {KeyCode::KeyR, "r"}, {KeyCode::KeyS, "s"}, {KeyCode::KeyT, "t"},
      {KeyCode::KeyU, "u"}, {KeyCode::KeyV, "v"}, {KeyCode::KeyW, "w"}, {KeyCode::KeyX, "x"},
      {KeyCode::KeyY, "y"}, {KeyCode::KeyZ, "z"},
      {KeyCode::Tab, "tab"}, {KeyCode::Backspace, "backspace"},
      {KeyCode::F1, "f1"}, {KeyCode::F2, "f2"}, {KeyCode::F3, "f3"}, {KeyCode::F4, "f4"},
      {KeyCode::F5, "f5"}, {KeyCode::F6, "f6"}, {KeyCode::F7, "f7"}, {KeyCode::F8, "f8"},
      {KeyCode::F9, "f9"}, {KeyCode::F10, "f10"}, {KeyCode::F11, "f11"}, {KeyCode::F12, "f12"},
    }};
  }

  std::optional<KeyCode> keyCodeFromString(std::string_view name) {
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "return") lower = "enter";
    if (lower == "esc") lower = "escape";

    for (const auto& entry : KEY_NAMES)
      if (entry.name == lower) return entry.code;
    return std::nullopt;
  }

  std::string_view keyCodeToString(KeyCode key) {
    for (const auto& entry : KEY_NAMES)
      if (entry.code == key) return entry.name;
    return "unknown";
  }
}
