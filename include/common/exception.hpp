#ifndef EXECEPTION_HPP
  #define EXECEPTION_HPP
#include <exception>
#include <string>

namespace rtype {
class RtypeError : public std::exception {
public:
  RtypeError(std::string const &message) : _message(message) {}
  const char *what() const noexcept override { return _message.c_str(); }

private:
  std::string _message;
};
}

#endif /* EXECEPTION_HPP */
