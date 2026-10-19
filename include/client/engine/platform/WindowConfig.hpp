#ifndef RTYPE_ENGINE_WINDOWCONFIG_HPP
#define RTYPE_ENGINE_WINDOWCONFIG_HPP

#include <string>

namespace rtype::engine {
  struct WindowConfig {
    unsigned int width{1280};
    unsigned int height{720};
    std::string title{"Game"};
    unsigned int framerateLimit{60};
    bool vsync{false};
  };
}

#endif // !RTYPE_ENGINE_WINDOWCONFIG_HPP
