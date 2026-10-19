#ifndef RTYPE_BACKEND_BACKENDS_HPP
#define RTYPE_BACKEND_BACKENDS_HPP

#include "engine/platform/BackendFactory.hpp"

namespace rtype::backend {
  void registerBuiltinBackends(engine::BackendFactory& factory);
}

#endif // !RTYPE_BACKEND_BACKENDS_HPP
