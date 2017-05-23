#ifndef _CONFUNDOSOCKET_HPP
#define _CONFUNDOSOCKET_HPP

#include "udpsocket.hpp"
#include "udpmux.hpp"

#include <cstdint>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <vector>

#define PAYLOAD      512
#define SERVERISN    4321
#define CLIENTISN    12345
#define CWNDINIT     512
#define SSTHRESHINIT 10000

enum cf_state {
  LISTEN = 0,
  SYN_SENT,
  SYN_RECEIVED,
  ESTABLISHED,
  ACK_ALL,
  FIN_WAIT, // client waiting for fin to be ACKed by server
  LAST_ACK, // server sending fin and expecting ACK
  TIME_WAIT, // client responds to all FINs from server with ACK
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
  CFP(UDPMux& udpmux, uint16_t id, const std::string& directory); // server
  CFP(UDPMux& udpmux, const std::string& host, const std::string& port,
      std::array<uint8_t, 512> first_payload); // client
  CFP(CFP&& o);
  ~CFP();

  void event(uint8_t data[], size_t size);
  bool send(std::array<uint8_t, 512>& data);
  void close();
  const struct sockaddr* getsockaddr();

 private:
  bool send_packet(struct cf_header* hdr, uint8_t* payload, size_t plsize);
  void send_ack(uint32_t ack);

  void send_syn();
  void send_synack(struct cf_header* rx_hdr);
  void send_ack_payload(struct cf_header* rx_hdr);

  bool check_conn(struct cf_header* rx_hdr);
  bool handle_ack(struct cf_header* rx_hdr);
  void handle_payload(struct cf_packet* pkt, size_t pktsize);
  void handle_fin(struct cf_header* rx_hdr);

  void clean_una_buf();

  cf_state st;
  uint32_t snd_nxt; // sequence no. of next byte in the stream
  uint32_t snd_una; // earliest sequence no. that has been sent but not acked
  uint32_t rcv_nxt; // sequence no. of next byte expected to be recvd from peer
  uint16_t conn_id;
  std::vector<struct cf_packet> una_buf;

  uint32_t cwnd;
  uint32_t ssthresh;

  UDPMux& mux;
  std::ofstream ofile;

  std::array<uint8_t, 512> first_payload;
};

#endif // _CONFUNDOSOCKET_HPP
