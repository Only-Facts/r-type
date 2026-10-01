#include "cli.hpp"
#include "exception.hpp"
#include "variables.hpp"
#include "GameEngine.hpp"
#include "SFMLWindow.hpp"
#include <memory>
#include <iostream>

std::size_t parse_arguments(int argc, const char *argv[]) {
  (void)argc;
  for (std::size_t i = 1; argv[i]; i++) {
    std::string flag(argv[i]);

    if (flag == "--help" || flag == "-h") return print_usage();
  }
  return SUCCESS;
}

int main(int argc, const char *argv[]) {
  std::size_t bits = parse_arguments(argc, argv);

  if (bits & 0b011) return FAIL;
  if (bits & 0b100) return SUCCESS;

  try {
    std::unique_ptr<rtype::engine::IWindow> window = 
      std::make_unique<rtype::engine::SFMLWindow>(1920, 1080, "R-Type Client");

    rtype::engine::GameEngine engine(std::move(window));
    engine.run();

    return SUCCESS;
  } catch (const rtype::RtypeError& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return ERROR;
  } catch (const std::exception& e) {
    std::cerr << "Uncaught error: " << e.what() << std::endl;
    return FAIL;
  } catch (...) {
    std::cerr << "Uncaught error." << std::endl;
    return FAIL;
  }
}
