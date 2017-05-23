#ifndef _UDPMUX_HPP
#define _UDPMUX_HPP

#include <netinet/in.h>
#include <sys/socket.h>
#include <map>

#define MAXPACKET 524

class CFP;
class UDPSocket;
struct cf_packet;

class UDPMux {
 public:
  UDPMux(const UDPSocket& udpsock);
  UDPMux(UDPMux&& other) noexcept = default;
  ~UDPMux();

  void connect(CFP* proto, sockaddr_in* addr);
  void connect(CFP* proto, const std::string& host, const std::string& port);

  // returns 1 if couldn't find a CFP object to send message to
  // fills our addrout and addrlenout
  void send(CFP* proto, const uint8_t data[], size_t size) const;
  void deliver(const sockaddr_in* addr, uint8_t data[], size_t size) const;

 private:
  const UDPSocket& sock;
  std::map<CFP*, sockaddr_in> mux;
  std::map<std::pair<uint32_t, uint16_t>, CFP*> demux;

  std::pair<uint32_t, uint16_t> unpack_sockaddr(const sockaddr_in* addr) const;
};

#endif // _UDPMUX_HPP
