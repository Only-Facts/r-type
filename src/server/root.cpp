#include "cli.hpp"
#include "exception.hpp"
#include "variables.hpp"
#include <cstddef>

std::size_t parse_arguments(int argc, const char *argv[]) {
  (void)argc;
  for (std::size_t i = 1; argv[i]; i++) {
    std::string flag(argv[i]);

    if (flag == "--help" || flag == "-h") return print_usage();
    else return bad_usage();
  }
  return SUCCESS;
}

int main(int argc, const char *argv[]) {
  std::size_t bits = parse_arguments(argc, argv);

  if (bits & (FAIL | ERROR | SUCCESS)) [[ unlikely ]] { return (bits & (FAIL | ERROR)) ? FAIL : SUCCESS; }

  try {
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
