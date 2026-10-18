#include "cli.hpp"
#include "exception.hpp"
#include "input/SFMLInputManager.hpp"
#include "gfx/SFMLTextFactory.hpp"
#include "gfx/SFMLTextureManager.hpp"
#include "variables.hpp"
#include "GameEngine.hpp"
#include "GameScene.hpp"
#include "MenuScene.hpp"
#include "SFMLWindow.hpp"
#include <limits>
#include <memory>
#include <string>

#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080
#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 4242
#define FONT_PATH "assets/fonts/tuffy.ttf"

struct ClientOptions {
  std::size_t width = DEFAULT_WIDTH;
  std::size_t height = DEFAULT_HEIGHT;
  std::string host = DEFAULT_HOST;
  unsigned short port = DEFAULT_PORT;
};

std::size_t parse_arguments(int argc, const char *argv[], ClientOptions* options) {
  (void)argc;
  for (std::size_t i = 1; argv[i]; i++) {
    std::string flag(argv[i]);

    if (flag == "--help" || flag == "-h") return print_usage();
    else if (flag == "--resolution" || flag == "-r") {
      if (!argv[i + 1] || !argv[i + 2]) return bad_usage();
      options->width = std::stoul(argv[++i]);
      options->height = std::stoul(argv[++i]);
    }
    else if (flag == "--host" || flag == "-H") {
      if (!argv[i + 1]) return bad_usage();
      options->host = argv[++i];
    }
    else if (flag == "--port" || flag == "-p") {
      if (!argv[i + 1]) return bad_usage();
      const unsigned long port = std::stoul(argv[++i]);
      if (port == 0 || port > std::numeric_limits<unsigned short>::max()) return bad_usage();
      options->port = static_cast<unsigned short>(port);
    }
    else return bad_usage();
  }
  return SUCCESS;
}

int main(int argc, const char *argv[]) {
  ClientOptions options;
  std::size_t bits = parse_arguments(argc, argv, &options);

  if (bits & (FAIL | ERROR | HELP)) [[ unlikely ]] return (bits & (FAIL | ERROR)) ? FAIL : SUCCESS;

  try {
    rtype::engine::SFMLTextFactory textFactory(FONT_PATH);
    auto window = std::make_unique<rtype::engine::SFMLWindow>(options.width, options.height, "R-Type Client");
    auto inputManager = std::make_unique<rtype::engine::SFMLInputManager>();
    auto textureManager = std::make_unique<rtype::engine::SFMLTextureManager>();

    info(("Initializing SFML window: " + std::string(BOLD) + std::string(ITALIC) + std::string(BLUE) + std::to_string(options.width) + "x" + std::to_string(options.height)).c_str());

    rtype::engine::GameEngine engine(std::move(window), std::move(inputManager), std::move(textureManager));

    auto& scenes = engine.getSceneManager();
    auto& textureManagerRef = engine.getTextureManager();
    auto& inputManagerRef = engine.getInputManager();

    auto backToMenu = [&scenes]() { scenes.changeScene("menu"); };
    auto startGame = [&](const std::string& host, unsigned short port) {
      scenes.addScene("game", std::make_shared<rtype::engine::GameScene>(
        textureManagerRef, inputManagerRef, textFactory, host, port, backToMenu));
      scenes.changeScene("game");
    };

    scenes.addScene("menu", std::make_shared<rtype::engine::MenuScene>(
      inputManagerRef, textFactory, options.host, options.port, startGame, [&scenes]() { scenes.requestQuit(); }));
    scenes.changeScene("menu");

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
