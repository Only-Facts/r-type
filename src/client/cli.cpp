#include "variables.hpp"
#include <cstddef>
#include <iostream>
#include <ostream>

std::size_t print_usage(std::ostream& os = std::cout) {
  os <<
    BLACK << ":: " << BOLD << BLUE << "Usage" << RESET << BLACK << ":\n" << RESET <<
    "   " << BOLD << GREEN << "r-type_client" << RESET << " [OPTIONS]\n\n" <<
    BLACK << ":: " << BOLD << BLUE << "Options" << RESET << BLACK << ":\n" << RESET <<
    "   " << BOLD << "-h, --help" << RESET << "                   Display this help message.\n"
    "   " << BOLD << "-r, --resolution <w> <h>" << RESET << "     Width & Height of the window resolution.       " << BLACK << "(default: from config)\n" << RESET <<
    "   " << BOLD << "-c, --config <file>" << RESET << "          Client configuration file.                     " << BLACK << "(default: assets/config/client.cfg)\n" << RESET
  ;
  return HELP;
}
