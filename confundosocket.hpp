#ifndef _CONFUNDOSOCKET_HPP
#define _CONFUNDOSOCKET_HPP

#include "udpsocket.hpp"

#include <cstdint>
#include <string>

#define PAYLOAD   512
#define SERVERSYN 4321
#define CLIENTSYN 12345

struct cf_header {
  uint32_t seq;        /* sequence number              */
  uint32_t ack;        /* acknowledgement number       */
  uint16_t conn;       /* connection id                */
  union {
    struct {
      uint16_t xxx: 13;    /* (unused, should be zero)     */
      uint16_t ack_f: 1;   /* acknowledge packet received  */
      uint16_t syn_f: 1;   /* synchronize sequence numbers */
      uint16_t fin_f: 1;   /* no more data from sender     */
    };
    uint16_t flgs;
  };
};

struct cf_packet {
  struct cf_header hdr;
  uint8_t payload[PAYLOAD];
};

class ConfundoSocket {
 public:
  ConfundoSocket(const std::string& host, const std::string& port);
  explicit ConfundoSocket(const std::string& port);

  void send_all(const std::string& data);
  std::string receive();

 private:
  void connect();
  void listen();
  void send_header();
  void send_packet(const uint8_t data[], size_t size);
  UDPSocket sock;
  struct cf_header tx_hdr;
};

#endif // _CONFUNDOSOCKET_HPP
