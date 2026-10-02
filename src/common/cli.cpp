#include "variables.hpp"
#include <iostream>
#include <ostream>

void error(const char *msg) {
  std::cerr << BLACK << ":: " << BOLD << RED << "ERROR" << RESET << BLACK << " - " << msg << RESET << std::endl;
}
 
void warn(const char *msg) {
  std::cerr << BLACK << ":: " << BOLD << YELLOW << "WARN" << RESET << BLACK << " - " << msg << RESET << std::endl;
}
 
void info(const char *msg) {
  std::cerr << BLACK << ":: " << BOLD << GREEN << "INFO" << RESET << BLACK << " - " << msg << RESET << std::endl;
}
 
void debug(const char *msg) {
  std::cerr << BLACK << ":: " << BOLD << CYAN << "DEBUG" << RESET << BLACK << " - " << msg << RESET << std::endl;
}

std::size_t print_usage(std::ostream& os = std::cout) {
  os <<
    BLACK << ":: " << BOLD << BLUE << "Usage" << RESET << BLACK << ":\n" << RESET <<
    "   " << BOLD << GREEN << "r-type_server" << RESET << " [OPTIONS]\n" <<
    "   " << BOLD << GREEN << "r-type_client" << RESET << " [OPTIONS]\n\n" <<
    BLACK << ":: " << BOLD << BLUE << "Options" << RESET << BLACK << ":\n" << RESET <<
    "   " << BOLD << "-h, --help" << RESET << "  Display this help message."
    << std::endl;
  return HELP;
}

std::size_t bad_usage(std::ostream& os = std::cerr) {
  error("Bad Usage.\n");
  print_usage(os);
  return ERROR;
}
