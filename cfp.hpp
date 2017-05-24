#ifndef _CONFUNDOSOCKET_HPP
#define _CONFUNDOSOCKET_HPP

#include "timer.hpp"
#include "udpmux.hpp"
#include "udpsocket.hpp"

#include <cstdint>
#include <deque>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <vector>

#define PAYLOAD      512
#define SERVERISN    4321
#define CLIENTISN    12345
#define CWNDINIT     512
#define SSTHRESHINIT 10000
#define RTO          0.5
#define FINWAITTIME  2
#define DISCONNECTTO 10

enum cf_state {
  LISTEN,
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
      uint16_t ack_f: 1;   /* acknowledge packet received  */
      uint16_t syn_f: 1;   /* synchronize sequence numbers */
      uint16_t fin_f: 1;   /* no more data from sender     */
      uint16_t xxx: 13;    /* (unused, should be zero)     */
    };
    uint16_t flgs;
  };
};

struct cf_packet {
  struct cf_header hdr;
  uint8_t payload[PAYLOAD];
};

using PayloadT = std::pair<std::array<uint8_t, 512>, size_t>;

class connection_closed : public std::runtime_error {
 public:
  explicit connection_closed(CFP& which, const char* what)
      : std::runtime_error(what), which{which} {};
  CFP& which;
};

class connection_closed_gracefully : public connection_closed {
 public:
  explicit connection_closed_gracefully(CFP& which)
      : connection_closed(which, "connection closed gracefully") {}
};
class connection_closed_ungracefully : public connection_closed {
  using connection_closed::connection_closed;
};

class EventLoop;
class CFP {
 friend EventLoop;
 public:
  CFP(const UDPMux& udpmux, uint16_t id, const std::string& directory); // server
  CFP(const UDPMux& udpmux, PayloadT first_pl); // client
  CFP(CFP&& o);
  ~CFP();

  void recv_event(uint8_t data[], size_t size);
  void timeout_event(); // RTO went off
  void disconnect_event(); // 
  void start();
  bool send(PayloadT buf);
  void close();
  const struct sockaddr* getsockaddr();

 private:
  bool send_packet(const struct cf_header* hdr, uint8_t* payload, size_t plsize);
  void send_packet_nocc(const struct cf_packet* pkt, size_t size, bool resnd);
  void send_ack(uint32_t ack);
  void send_fin();

  void send_syn();
  void send_synack(struct cf_header* rx_hdr);
  void send_ack_payload(struct cf_header* rx_hdr);

  bool check_conn(struct cf_header* rx_hdr);
  bool handle_ack(struct cf_header* rx_hdr);
  void handle_payload(struct cf_packet* pkt, size_t pktsize);
  void handle_fin(struct cf_header* rx_hdr);

  void set_first_payload(PayloadT buf);
  void clean_una_buf();

  void terminate_gracefully();
  void terminate_ungracefully(const std::string& why);

  cf_state st;
  uint32_t snd_nxt; // sequence no. of next byte in the stream
  uint32_t snd_una; // earliest sequence no. that has been sent but not acked
  uint32_t rcv_nxt; // sequence no. of next byte expected to be recvd from peer
  uint16_t conn_id;
  std::deque<std::pair<struct cf_packet, size_t>> una_buf;

  uint32_t cwnd;
  uint32_t ssthresh;

  const UDPMux& mux;

  std::ofstream ofile;
  std::string directory;

  PayloadT first_payload;
  Timer rto_timer;
  Timer disconnect_timer;
};

#endif // _CONFUNDOSOCKET_HPP
