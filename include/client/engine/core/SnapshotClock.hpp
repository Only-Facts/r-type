#ifndef RTYPE_ENGINE_SNAPSHOTCLOCK_HPP
#define RTYPE_ENGINE_SNAPSHOTCLOCK_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace rtype::engine {
  class SnapshotClock {
  public:
    SnapshotClock() = default;

    SnapshotClock(double tickRate, double delaySeconds, double maxExtrapolationSeconds) {
      configure(tickRate, delaySeconds, maxExtrapolationSeconds);
    }

    void configure(double tickRate, double delaySeconds, double maxExtrapolationSeconds) {
      _tickRate = tickRate > 0.0 ? tickRate : DEFAULT_TICK_RATE;
      _delay = std::max(delaySeconds, 0.0);
      _maxExtrapolation = std::max(maxExtrapolationSeconds, 0.0);
    }

    void advance(float dt) {
      if (std::isfinite(dt) && dt > 0.0f) _localTime += static_cast<double>(dt);
    }

    void observe(std::uint32_t tick) {
      const double sampleOffset = static_cast<double>(tick) / _tickRate - _localTime;
      if (!_synchronized || std::abs(sampleOffset - _offset) > RESYNC_SECONDS) {
        _offset = sampleOffset;
        _synchronized = true;
        _hasRenderTick = false;
        return;
      }
      _offset += (sampleOffset - _offset) * OFFSET_SMOOTHING;
    }

    double nextRenderTick() {
      const double raw = (_localTime + _offset - _delay) * _tickRate;
      if (!_hasRenderTick || raw > _lastRenderTick) {
        _lastRenderTick = raw;
        _hasRenderTick = true;
      }
      return _lastRenderTick;
    }

    [[nodiscard]] std::optional<double> currentRenderTick() const {
      if (!_hasRenderTick) return std::nullopt;
      return _lastRenderTick;
    }

    [[nodiscard]] bool isSynchronized() const { return _synchronized; }
    [[nodiscard]] double tickRate() const { return _tickRate; }
    [[nodiscard]] double delaySeconds() const { return _delay; }
    [[nodiscard]] double maxExtrapolationTicks() const { return _maxExtrapolation * _tickRate; }

  private:
    static constexpr double DEFAULT_TICK_RATE = 60.0;
    static constexpr double OFFSET_SMOOTHING = 0.05;
    static constexpr double RESYNC_SECONDS = 0.5;

    double _tickRate{DEFAULT_TICK_RATE};
    double _delay{0.1};
    double _maxExtrapolation{0.1};
    double _localTime{0.0};
    double _offset{0.0};
    bool _synchronized{false};
    double _lastRenderTick{0.0};
    bool _hasRenderTick{false};
  };
}

#endif // !RTYPE_ENGINE_SNAPSHOTCLOCK_HPP
