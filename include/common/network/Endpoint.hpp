#ifndef ENDPOINT_HPP
#define ENDPOINT_HPP

#include <string>

namespace rtype::network {
  struct Endpoint {
    std::string address;
    unsigned short port{0};
  };
}

#endif // !ENDPOINT_HPP
