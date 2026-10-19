#ifndef RTYPE_GAME_DIFFICULTY_HPP
#define RTYPE_GAME_DIFFICULTY_HPP

#include "engine/config/Config.hpp"
#include "engine/core/Session.hpp"
#include "engine/gfx/GfxTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rtype::game {
  struct DifficultyLevel {
    std::string id;
    std::string label;
    engine::Color color{255, 255, 255, 255};
  };

  class Difficulty {
  public:
    static constexpr const char* SESSION_KEY = "difficulty";

    void load(const engine::Config& config, const std::string& section);

    std::size_t count() const { return _levels.size(); }
    const DifficultyLevel& at(std::size_t index) const;
    std::size_t defaultIndex() const { return _defaultIndex; }

    std::size_t selected(const engine::Session& session) const;
    void select(engine::Session& session, std::size_t index) const;

  private:
    std::vector<DifficultyLevel> _levels;
    std::size_t _defaultIndex{0};
  };
}

#endif // !RTYPE_GAME_DIFFICULTY_HPP
