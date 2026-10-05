#ifndef INETWORKSOCKET_HPP
#define INETWORKSOCKET_HPP

#include "Endpoint.hpp"
#include "Packet.hpp"

namespace rtype::network {
  class INetworkSocket {
  public:
    virtual ~INetworkSocket() = default;

    virtual void bind(unsigned short port = 0) = 0;
    virtual void setNonBlocking(bool nonBlocking) = 0;
    virtual void send(const PacketData& data, const Endpoint& endpoint) = 0;
    virtual bool receive(PacketData& data, Endpoint& outEndpoint) = 0;
    virtual unsigned short getLocalPort() const = 0;
  };
}

#endif // !INETWORKSOCKET_HPP
