#ifndef RTYPE_ENGINE_BACKENDFACTORY_HPP
#define RTYPE_ENGINE_BACKENDFACTORY_HPP

#include "engine/platform/IBackend.hpp"
#include "engine/platform/WindowConfig.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::engine {
  /// Creates backends by name (`[engine] backend = sfml` in the config).
  class BackendFactory {
  public:
    using Creator = std::function<std::unique_ptr<IBackend>(const WindowConfig&)>;

    void registerBackend(const std::string& name, Creator creator);
    bool has(const std::string& name) const;
    std::unique_ptr<IBackend> create(const std::string& name, const WindowConfig& config) const;
    std::vector<std::string> getAvailable() const;

  private:
    std::unordered_map<std::string, Creator> _creators;
  };
}

#endif // !RTYPE_ENGINE_BACKENDFACTORY_HPP
