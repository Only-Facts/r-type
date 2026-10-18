#include "backend/Backends.hpp"
#include "backend/sfml/SFMLBackend.hpp"

namespace rtype::backend {
  void registerBuiltinBackends(engine::BackendFactory& factory) {
    factory.registerBackend("sfml", [](const engine::WindowConfig& config) {
      return std::make_unique<SFMLBackend>(config);
    });
  }
}
