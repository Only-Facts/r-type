#ifndef RTYPE_ENGINE_FRAMESTATS_HPP
#define RTYPE_ENGINE_FRAMESTATS_HPP

#include <map>
#include <string>

namespace rtype::engine {
  struct FrameStats {
    float fps{0.0f};
    float frameTimeMs{0.0f};
  };

  struct DebugState {
    bool overlayVisible{false};
    std::map<std::string, std::string> lines;
  };
}

#endif // !RTYPE_ENGINE_FRAMESTATS_HPP
