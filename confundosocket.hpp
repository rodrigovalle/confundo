#ifndef _CONFUNDOSOCKET_HPP
#define _CONFUNDOSOCKET_HPP

#include "udpconnection.hpp"

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
  static ConfundoSocket accept(const UDPSocket& socket, uint16_t id);
  static ConfundoSocket connect(const UDPSocket& socket,
                                const std::string& hostname,
                                const std::string& port); 

  void send_all(const std::string& data);
  std::string receive();

 private:
  ConfundoSocket(UDPConnection conn, struct cf_header* syn_hdr, uint16_t id);
  ConfundoSocket(UDPConnection conn);
  inline void send_header(const struct cf_header* hdr);
  inline void recv_header(struct cf_header* hdr);
  void send_payload(const uint8_t data[], size_t size);
  size_t recv_payload(uint8_t payload[PAYLOAD], size_t size);

  UDPConnection sock;
  uint32_t snd_nxt; // sequence no. of next byte in the stream
  uint32_t snd_una; // earliest sequence no. that has been sent but not acked
  uint32_t rcv_nxt; // sequence no. of next byte expected to be recvd from peer
  uint16_t conn_id;
};

#endif // _CONFUNDOSOCKET_HPP
