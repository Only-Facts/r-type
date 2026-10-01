#include "variables.hpp"
#include <iostream>

std::size_t print_usage(void) {
  std::cout <<
    ":: Usage:\n" <<
    "   r-type_server [OPTIONS]\n" <<
    "   r-type_client [OPTIONS]\n\n" <<
    ":: Options:\n" <<
    "   -h, --help  Display this help message."
    << std::endl;
  return HELP;
}
