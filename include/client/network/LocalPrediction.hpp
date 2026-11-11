#ifndef RTYPE_CLIENT_LOCALPREDICTION_HPP
#define RTYPE_CLIENT_LOCALPREDICTION_HPP

#include "gameplay/PlayerMovement.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>

namespace rtype::client {
  class LocalPrediction {
  public:
    struct Settings {
      std::size_t maxPendingInputs{120};
      float correctionSpeed{12.0f};
      float snapDistance{120.0f};
      gameplay::MovementBounds bounds{};
    };

    void configure(const Settings& settings) {
      _settings = settings;
      _settings.maxPendingInputs = std::max<std::size_t>(_settings.maxPendingInputs, 1);
      _settings.correctionSpeed = std::max(_settings.correctionSpeed, 0.0f);
      _settings.snapDistance = std::max(_settings.snapDistance, 0.0f);
    }

    void reset(gameplay::MovementVector position) {
      _active = true;
      _predicted = position;
      _offset = {};
      _lastCorrection = 0.0f;
      _pending.clear();
    }

    void stop() {
      _active = false;
      _offset = {};
      _pending.clear();
    }

    void applyInput(std::uint32_t sequence, const gameplay::MovementInput& input) {
      if (!_active) return;
      _predicted = gameplay::applyMovement(_predicted, input, gameplay::INPUT_STEP_SECONDS, _settings.bounds);
      _pending.push_back({sequence, input});
      while (_pending.size() > _settings.maxPendingInputs) _pending.pop_front();
    }

    void reconcile(std::uint32_t lastProcessedInput, gameplay::MovementVector confirmed) {
      if (!_active) return;
      while (!_pending.empty() && !isNewer(_pending.front().sequence, lastProcessedInput)) _pending.pop_front();

      gameplay::MovementVector corrected = confirmed;
      for (const auto& pending : _pending)
        corrected = gameplay::applyMovement(corrected, pending.input, gameplay::INPUT_STEP_SECONDS, _settings.bounds);

      const float dx = _predicted.x - corrected.x;
      const float dy = _predicted.y - corrected.y;
      _lastCorrection = std::hypot(dx, dy);
      if (_lastCorrection > _settings.snapDistance) {
        _offset = {};
      } else {
        _offset.x += dx;
        _offset.y += dy;
      }
      _predicted = corrected;
    }

    void update(float dt) {
      if (!std::isfinite(dt) || dt <= 0.0f) return;
      const float keep = std::exp(-_settings.correctionSpeed * dt);
      _offset.x *= keep;
      _offset.y *= keep;
      if (std::hypot(_offset.x, _offset.y) < MIN_OFFSET) _offset = {};
    }

    [[nodiscard]] bool active() const { return _active; }
    [[nodiscard]] gameplay::MovementVector predicted() const { return _predicted; }
    [[nodiscard]] gameplay::MovementVector displayed() const {
      return {_predicted.x + _offset.x, _predicted.y + _offset.y};
    }
    [[nodiscard]] std::size_t pendingCount() const { return _pending.size(); }
    [[nodiscard]] float lastCorrection() const { return _lastCorrection; }

  private:
    struct PendingInput {
      std::uint32_t sequence;
      gameplay::MovementInput input;
    };

    static constexpr float MIN_OFFSET = 0.01f;

    static bool isNewer(std::uint32_t candidate, std::uint32_t reference) {
      const std::uint32_t delta = candidate - reference;
      return delta != 0 && delta < 0x80000000u;
    }

    Settings _settings;
    bool _active{false};
    gameplay::MovementVector _predicted;
    gameplay::MovementVector _offset;
    float _lastCorrection{0.0f};
    std::deque<PendingInput> _pending;
  };
}

#endif // !RTYPE_CLIENT_LOCALPREDICTION_HPP
