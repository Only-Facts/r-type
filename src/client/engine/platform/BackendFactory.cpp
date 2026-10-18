#include "engine/platform/BackendFactory.hpp"
#include "exception.hpp"
#include <algorithm>

namespace rtype::engine {
  void BackendFactory::registerBackend(const std::string& name, Creator creator) {
    _creators[name] = std::move(creator);
  }

  bool BackendFactory::has(const std::string& name) const {
    return _creators.find(name) != _creators.end();
  }

  std::unique_ptr<IBackend> BackendFactory::create(const std::string& name, const WindowConfig& config) const {
    auto it = _creators.find(name);
    if (it == _creators.end()) {
      std::string available;
      for (const auto& backend : getAvailable()) available += (available.empty() ? "" : ", ") + backend;
      throw RtypeError("Unknown backend '" + name + "' (available: " + available + ")");
    }
    return it->second(config);
  }

  std::vector<std::string> BackendFactory::getAvailable() const {
    std::vector<std::string> names;
    for (const auto& [name, creator] : _creators) names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
  }
}
