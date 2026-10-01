#include "variables.hpp"
#include "exception.hpp"
#include <cstddef>
#include <iostream>

std::size_t print_usage(void) {
  std::cout <<
    ":: Usage:\n" <<
    "   r-type_server [OPTIONS]\n\n" <<
    ":: Options:\n" <<
    "   -h, --help  Display this help message."
    << std::endl;
  return HELP;
}

std::size_t parse_arguments(int argc, const char *argv[]) {
  (void)argc;
  for (std::size_t i = 1; argv[i]; i++) {
    std::string flag(argv[i]);

    if (flag == "--help" || flag == "-h") return print_usage();
  }
  return SUCCESS;
}

int main (int argc, const char *argv[]) {
  std::size_t bits = parse_arguments(argc, argv);

  if (bits & 0b11) return FAIL;
  if (bits & 0b100) return SUCCESS;

  try {
    return SUCCESS;
  } catch (const rtype::RtypeError& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return ERROR;
  } catch (const std::exception& e) {
    std::cerr << "Uncaught error: " << e.what() << std::endl;
    return ERROR;
  } catch (...) {
    std::cerr << "Uncaught error." << std::endl;
    return ERROR;
  }
}
