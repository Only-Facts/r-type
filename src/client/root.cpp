#include "cli.hpp"
#include "exception.hpp"
#include "input/SFMLInputManager.hpp"
#include "gfx/SFMLTextureManager.hpp"
#include "variables.hpp"
#include "GameEngine.hpp"
#include "GameScene.hpp"
#include "SFMLWindow.hpp"
#include <memory>

#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080

std::size_t parse_arguments(int argc, const char *argv[], std::size_t* width, std::size_t* height) {
  (void)argc;
  for (std::size_t i = 1; argv[i]; i++) {
    std::string flag(argv[i]);

    if (flag == "--help" || flag == "-h") return print_usage();
    else if (flag == "--resolution" || flag == "-r") {
      if (!argv[i + 1] || !argv[i + 2]) return bad_usage();
      *width = std::stoul(argv[++i]);
      *height = std::stoul(argv[++i]);
    }
    else return bad_usage();
  }
  return SUCCESS;
}

int main(int argc, const char *argv[]) {
  std::size_t width = DEFAULT_WIDTH;
  std::size_t height = DEFAULT_HEIGHT;
  std::size_t bits = parse_arguments(argc, argv, &width, &height);

  if (bits & (FAIL | ERROR | HELP)) [[ unlikely ]] return (bits & (FAIL | ERROR)) ? FAIL : SUCCESS;

  try {
    auto window = std::make_unique<rtype::engine::SFMLWindow>(width, height, "R-Type Client");
    auto inputManager = std::make_unique<rtype::engine::SFMLInputManager>();
    auto textureManager = std::make_unique<rtype::engine::SFMLTextureManager>();

    info(("Initializing SFML window: " + std::string(BOLD) + std::string(ITALIC) + std::string(BLUE) + std::to_string(width) + "x" + std::to_string(height)).c_str());

    rtype::engine::GameEngine engine(std::move(window), std::move(inputManager), std::move(textureManager));

    auto gameScene = std::make_shared<rtype::engine::GameScene>(
      engine.getTextureManager(),
      engine.getInputManager()
    );

    engine.getSceneManager().addScene("game", gameScene);
    engine.getSceneManager().changeScene("game");

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
