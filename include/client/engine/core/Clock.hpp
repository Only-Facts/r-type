#ifndef RTYPE_ENGINE_CLOCK_HPP
#define RTYPE_ENGINE_CLOCK_HPP

#include <chrono>

namespace rtype::engine {
  class Clock {
  public:
    explicit Clock(float maxDelta = 0.25f)
      : _maxDelta(maxDelta), _last(std::chrono::steady_clock::now()) {}

    float tick() {
      const auto now = std::chrono::steady_clock::now();
      const float dt = std::chrono::duration<float>(now - _last).count();
      _last = now;
      return dt > _maxDelta ? _maxDelta : dt;
    }

    void reset() { _last = std::chrono::steady_clock::now(); }

  private:
    float _maxDelta;
    std::chrono::steady_clock::time_point _last;
  };
}

#endif // !RTYPE_ENGINE_CLOCK_HPP
