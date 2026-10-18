#include "variables.hpp"
#include <iostream>
#include <ostream>

std::size_t print_usage(std::ostream& os = std::cout) {
  os <<
    BLACK << ":: " << BOLD << BLUE << "Usage" << RESET << BLACK << ":\n" << RESET <<
    "   " << BOLD << GREEN << "r-type_client" << RESET << " [OPTIONS]\n\n" <<
    BLACK << ":: " << BOLD << BLUE << "Options" << RESET << BLACK << ":\n" << RESET <<
    "   " << BOLD << "-h, --help" << RESET << "                   Display this help message.\n"
    "   " << BOLD << "-r, --resolution <w> <h>" << RESET << "     Width & Height of the window resolution.       " << BLACK << "(default: 1920 1080)\n" << RESET <<
    "   " << BOLD << "-H, --host <address>" << RESET << "         Server address pre-filled in the menu.         " << BLACK << "(default: 127.0.0.1)\n" << RESET <<
    "   " << BOLD << "-p, --port <port>" << RESET << "            Server port pre-filled in the menu.            " << BLACK << "(default: 4242)\n"
  ;
  return HELP;
}

