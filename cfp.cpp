#include "cfp.hpp"
#include "udpsocket.hpp"
#include "util.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

// convert to network byte order
static void host_to_net(struct cf_header* hdr) {
  hdr->seq = htonl(hdr->seq);
  hdr->ack = htonl(hdr->ack);
  hdr->conn = htons(hdr->conn);
  hdr->flgs = htons(hdr->flgs);
}

// convert from network byte order
static void net_to_host(struct cf_header* hdr) {
  hdr->seq = ntohl(hdr->seq);
  hdr->ack = ntohl(hdr->ack);
  hdr->conn = ntohs(hdr->conn);
  hdr->flgs = ntohs(hdr->flgs);
}

// server constructor (is not connected at start, must connect yourself)
CFP::CFP(UDPMux& udpmux, uint16_t id)
    : st{LISTEN}, snd_una{0}, conn_id{id}, cwnd{CWNDINIT},
      ssthresh{SSTHRESHINIT}, mux{udpmux} {
}

// client constructor (automatically connects)
CFP::CFP(UDPMux& udpmux, const std::string& host, const std::string& port)
  : st{SYN_SENT}, snd_una{0}, conn_id{0}, cwnd{CWNDINIT},
    ssthresh{SSTHRESHINIT}, mux{udpmux} {

  mux.connect(this, host, port);
  send_syn();
}

CFP::~CFP() {}

void CFP::event(uint8_t data[], size_t size) {
  struct cf_header* rx_hdr;
  struct cf_packet* pkt = reinterpret_cast<struct cf_packet*>(data);
  net_to_host(&pkt->hdr);
  report(RECV, &pkt->hdr, 0, 0);

  switch (st) {
    case LISTEN:
      if (!(pkt->hdr.syn_f)) {
        // refuse the connection
        // break;
        throw std::runtime_error("received non-syn packet");
      }
      st = SYN_RECEIVED;
      send_synack(pkt, size);
      break;

    case SYN_SENT:
      rx_hdr = &pkt->hdr;
      if (!(rx_hdr->syn_f && rx_hdr->ack_f && rx_hdr->ack == snd_nxt)) {
        // break;
        throw std::runtime_error("server did not respond with syn-ack");
      }
      st = ESTABLISHED;
      send_ack_payload(pkt, size);
      break;

    case SYN_RECEIVED:
      rx_hdr = &pkt->hdr;
      if (!(rx_hdr->ack_f && rx_hdr->ack == rcv_nxt)) {
        // break;
        throw std::runtime_error("client did not respond with ack");
      }
      st = ESTABLISHED;
      rx_hdr->ack = false;

    case ESTABLISHED:
      // start sending messages from userspace
      handle_recv(pkt, size);
      break;

    case FIN_WAIT_1:
      break;
    case FIN_WAIT_2:
      break;
    case CLOSE_WAIT:
      break;
    case CLOSING:
      break;
    case LAST_ACK:
      break;
    case TIME_WAIT:
      break;
    case CLOSED:
      break;
  }
}

void CFP::send(std::array<uint8_t, 512>& data) {
  presnd_queue.emplace(data);
}

void CFP::close() {}

// client initiates connection
// TODO: timeout
void CFP::send_syn() {
  struct cf_header tx_hdr = {};

  snd_nxt = CLIENTSYN + 1;

  // send syn
  tx_hdr.syn_f = true;
  tx_hdr.seq = CLIENTSYN;
  send_packet(&tx_hdr, nullptr, 0);
}

// server sends a synack in response to client's syn
void CFP::send_synack(struct cf_packet* pkt, size_t size) {
  struct cf_header* rx_hdr = &pkt->hdr; // ignore payload
  struct cf_header tx_hdr = {};

  rcv_nxt = rx_hdr->seq + 1;
  snd_nxt = SERVERSYN + 1;

  // send syn-ack
  tx_hdr.syn_f = true;
  tx_hdr.ack_f = true;
  tx_hdr.seq = SERVERSYN;
  tx_hdr.ack = rcv_nxt;
  tx_hdr.conn = conn_id;
  send_packet(&tx_hdr, nullptr, 0);
}

// client sends first ACK with payload
void CFP::send_ack_payload(struct cf_packet* pkt, size_t size) {
  struct cf_header* rx_hdr = &pkt->hdr;
  struct cf_header tx_hdr = {};

  conn_id = rx_hdr->conn;
  rcv_nxt = rx_hdr->seq + 1;

  tx_hdr.ack_f = true;
  tx_hdr.seq = snd_nxt;
  tx_hdr.ack = rcv_nxt;
  tx_hdr.conn = conn_id;

  std::array<uint8_t, 512> payload = presnd_queue.front();
  presnd_queue.pop();
  send_packet(&tx_hdr, payload.data(), payload.size());
}

void CFP::handle_recv(struct cf_packet* pkt, size_t size) {
  struct cf_header* hdr = &pkt->hdr;
  if (hdr->ack_f && hdr->ack >= snd_una) {
    // XXX
  }
  if (hdr->seq == rcv_nxt) {
    // XXX
  }
}

// TODO: retry on timeout
void CFP::send_packet(struct cf_header* hdr, uint8_t* payload, size_t size) {
  struct cf_packet pkt = {};
  pkt.hdr = *hdr;
  memcpy(&pkt.payload, payload, size);

  report(SEND, &pkt.hdr, 0, 0); // TODO: fill in cwnd and ssthresh
  host_to_net(&pkt.hdr);
  mux.send(this, reinterpret_cast<uint8_t*>(&pkt), sizeof(struct cf_header) + size);

  snd_nxt += size;
  // XXX: snd_una = 
}
