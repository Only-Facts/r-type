#include "cli.hpp"
#include "engine/GameEngine.hpp"
#include "engine/config/Config.hpp"
#include "exception.hpp"
#include "game/GameScene.hpp"
#include "game/SkinScene.hpp"
#include "game/TitleScene.hpp"
#include "variables.hpp"
#include <charconv>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#define DEFAULT_CONFIG "assets/config/client.cfg"
#define MAX_DIMENSION 16384

namespace {
  struct LaunchOptions {
    std::string configPath{DEFAULT_CONFIG};
    std::optional<unsigned int> width;
    std::optional<unsigned int> height;
  };

  std::optional<unsigned int> parse_dimension(const char* str) {
    unsigned int value = 0;
    const char* end = str + std::strlen(str);
    auto [ptr, ec] = std::from_chars(str, end, value);
    if (ec != std::errc{} || ptr != end || value == 0 || value > MAX_DIMENSION) return std::nullopt;
    return value;
  }

  std::size_t parse_arguments(int argc, const char* argv[], LaunchOptions& options) {
    for (int i = 1; i < argc; i++) {
      const std::string flag(argv[i]);

      if (flag == "--help" || flag == "-h") return print_usage();
      if (flag == "--resolution" || flag == "-r") {
        if (i + 2 >= argc) return bad_usage();
        options.width = parse_dimension(argv[++i]);
        options.height = parse_dimension(argv[++i]);
        if (!options.width || !options.height) return bad_usage();
      } else if (flag == "--config" || flag == "-c") {
        if (i + 1 >= argc) return bad_usage();
        options.configPath = argv[++i];
      } else {
        return bad_usage();
      }
    }
    return SUCCESS;
  }
}

int main(int argc, const char* argv[]) {
  LaunchOptions options;
  std::size_t bits = parse_arguments(argc, argv, options);

  if (bits & (FAIL | ERROR | HELP)) [[ unlikely ]] return (bits & (FAIL | ERROR)) ? FAIL : SUCCESS;

  try {
    auto config = rtype::engine::Config::fromFile(options.configPath);
    if (options.width) config.set("window", "width", std::to_string(*options.width));
    if (options.height) config.set("window", "height", std::to_string(*options.height));

    rtype::engine::GameEngine engine(std::move(config));
    const auto& cfg = engine.getConfig();

    info(("Initializing window: " + std::string(BOLD) + std::string(ITALIC) + std::string(BLUE) +
          cfg.getString("window", "width") + "x" + cfg.getString("window", "height") + std::string(RESET) +
          " (backend: " + engine.getBackendName() + ")").c_str());

    auto& scenes = engine.getSceneManager();
    scenes.addScene("title", std::make_shared<rtype::game::TitleScene>(engine.getContext()));
    scenes.addScene("skin", std::make_shared<rtype::game::SkinScene>(engine.getContext()));
    scenes.addScene("game", std::make_shared<rtype::game::GameScene>(engine.getContext()));
    scenes.changeScene(cfg.getString("engine", "start_scene", "title"));

    engine.run();
    return SUCCESS;
  } catch (const rtype::RtypeError& e) {
    error(e.what());
    return ERROR;
  } catch (const std::exception& e) {
    std::string msg = "Uncaught error: ";
    error(msg.append(e.what()).c_str());
    return FAIL;
  } catch (...) {
    error("Uncaught error.");
    return FAIL;
  }
}
