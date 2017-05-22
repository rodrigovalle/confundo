#ifndef _CONFUNDOSOCKET_HPP
#define _CONFUNDOSOCKET_HPP

#include "udpsocket.hpp"
#include "udpmux.hpp"

#include <cstdint>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <vector>

#define PAYLOAD      512
#define SERVERSYN    4321
#define CLIENTSYN    12345
#define CWNDINIT     512
#define SSTHRESHINIT 10000

enum cf_state {
  LISTEN = 0,
  SYN_SENT,
  SYN_RECEIVED,
  ESTABLISHED,
  FIN_WAIT_1,
  FIN_WAIT_2,
  CLOSE_WAIT,
  CLOSING,
  LAST_ACK,
  TIME_WAIT,
  CLOSED
};

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

class CFP {
 public:
  CFP(const UDPMux& udpmux, cf_state start, uint16_t id);
  ~CFP();

  void event(uint8_t data[], size_t size);
  void send(std::array<uint8_t, 512>& data);
  void close();
  const struct sockaddr* getsockaddr();

 private:
  void send_packet(struct cf_header* hdr, uint8_t* payload, size_t size);

  void send_syn();
  void send_synack(struct cf_packet* pkt, size_t size);
  void send_ack_payload(struct cf_packet* pkt, size_t size);
  void handle_recv(struct cf_packet* pkt, size_t size);

  cf_state st;
  uint32_t snd_nxt; // sequence no. of next byte in the stream
  uint32_t snd_una; // earliest sequence no. that has been sent but not acked
  uint32_t rcv_nxt; // sequence no. of next byte expected to be recvd from peer
  uint16_t conn_id;
  std::vector<struct cf_packet> una_buf;
  std::vector<struct cf_packet> rcv_buf;

  uint32_t cwnd;
  uint32_t ssthresh;

  const UDPMux& mux;

  std::queue<std::array<uint8_t, 512>> presnd_queue;
};

#endif // _CONFUNDOSOCKET_HPP
