#ifndef RTYPE_ENGINE_FRAMESTATS_HPP
#define RTYPE_ENGINE_FRAMESTATS_HPP

namespace rtype::engine {
  /// Performance metrics measured by the engine loop, averaged over a short window.
  struct FrameStats {
    float fps{0.0f};
    float frameTimeMs{0.0f};
  };

  /// Developer tools state, owned by the engine so it survives scene changes.
  struct DebugState {
    bool overlayVisible{false};
  };
}

#endif // !RTYPE_ENGINE_FRAMESTATS_HPP
