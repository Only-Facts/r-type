#ifndef RTYPE_BACKEND_BACKENDS_HPP
#define RTYPE_BACKEND_BACKENDS_HPP

#include "engine/platform/BackendFactory.hpp"

namespace rtype::backend {
  /// Registers every backend compiled into the client ("sfml" for now).
  /// Adding a backend = implementing IBackend + one line in Backends.cpp.
  void registerBuiltinBackends(engine::BackendFactory& factory);
}

#endif // !RTYPE_BACKEND_BACKENDS_HPP
