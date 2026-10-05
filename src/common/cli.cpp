#include "variables.hpp"
#include "cli.hpp"
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

std::size_t bad_usage(std::ostream& os) {
  error("Bad Usage.\n");
  print_usage(os);
  return ERROR;
}
