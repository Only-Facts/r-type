#include "variables.hpp"
#include <iostream>
#include <ostream>

inline void error(const char* msg) {
  std::cerr << ":: ERROR - " << msg << std::endl;
}
 
inline void warn(const char* msg) {
  std::cerr << ":: WARN - " << msg << std::endl;
}
 
inline void info(const char* msg) {
  std::cerr << ":: INFO - " << msg << std::endl;
}
 
inline void debug(const char* msg) {
  std::cerr << ":: DEBUG - " << msg << std::endl;
}

std::size_t print_usage(std::ostream& os = std::cout) {
  os <<
    ":: Usage:\n" <<
    "   r-type_server [OPTIONS]\n" <<
    "   r-type_client [OPTIONS]\n\n" <<
    ":: Options:\n" <<
    "   -h, --help  Display this help message."
    << std::endl;
  return HELP;
}

std::size_t bad_usage(std::ostream& os = std::cerr) {
  error("Bad Usage.\n");
  print_usage(os);
  return ERROR;
}
