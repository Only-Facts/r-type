#include "variables.hpp"
#include <iostream>
#include <ostream>

std::size_t print_usage(std::ostream& os = std::cout) {
  os <<
    BLACK << ":: " << BOLD << BLUE << "Usage" << RESET << BLACK << ":\n" << RESET <<
    "   " << BOLD << GREEN << "r-type_server" << RESET << " [OPTIONS]\n\n" <<
    BLACK << ":: " << BOLD << BLUE << "Options" << RESET << BLACK << ":\n" << RESET <<
    "   " << BOLD << "-h, --help" << RESET << "         Display this help message.\n"
    "   " << BOLD << "-p, --port <n>" << RESET << "     Local port n to bind.       " << BLACK << "(default: 4242)\n"
  ;
  return HELP;
}
