#include "cfp.hpp"
#include "udpsocket.hpp"
#include "util.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
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
CFP::CFP(UDPMux& udpmux, uint16_t id, const std::string& directory)
    : st{LISTEN}, conn_id{id}, cwnd{CWNDINIT}, ssthresh{SSTHRESHINIT},
      mux{udpmux}, ofile{directory + std::to_string(id) + ".file"} {}

// client constructor (automatically connects)
CFP::CFP(UDPMux& udpmux, const std::string& host, const std::string& port,
         PayloadT first_pl)
    : st{SYN_SENT}, conn_id{0}, cwnd{CWNDINIT}, ssthresh{SSTHRESHINIT},
      mux{udpmux}, first_payload{first_pl} {
  mux.connect(this, host, port);
  send_syn();
}

CFP::CFP(CFP&& o) : st{o.st}, snd_nxt{o.snd_nxt}, rcv_nxt{o.rcv_nxt},
    conn_id{o.conn_id}, una_buf{o.una_buf}, cwnd{o.cwnd}, ssthresh{o.ssthresh},
    mux{o.mux}, ofile{std::move(o.ofile)}, first_payload{o.first_payload} {}

CFP::~CFP() {}

void CFP::event(uint8_t data[], size_t size) {
  struct cf_packet* pkt = reinterpret_cast<struct cf_packet*>(data);
  net_to_host(&pkt->hdr);
  report(RECV, &pkt->hdr, cwnd, ssthresh);

  switch (st) {
    case LISTEN:
      send_synack(&pkt->hdr);
      st = SYN_RECEIVED;
      break;

    case SYN_SENT:
      send_ack_payload(&pkt->hdr);
      st = ESTABLISHED;
      break;

    case SYN_RECEIVED:
      if (!handle_ack(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0);
        break;
      }
      if (!check_conn(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0);
        break;
      }
      st = ESTABLISHED;
      handle_payload(pkt, size);
      break;

    case ESTABLISHED:
      if (!check_conn(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0);
        break;
      }
      handle_ack(&pkt->hdr);
      handle_payload(pkt, size);
      handle_fin(&pkt->hdr);
      break;

    case ACK_ALL:
      handle_ack(&pkt->hdr);
      if (snd_una == snd_nxt) {
        send_fin();
        st = FIN_WAIT;
      }
      break;

    case FIN_WAIT:
      // client sent fin, expects ACK
      if (!handle_ack(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0);
        break;
      }
      st = TIME_WAIT;
      break;

    case LAST_ACK:
      // server sent fin, expects ACK
      st = CLOSED;
      ofile.close();
      if (!handle_ack(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0);
        break;
      }
      break;

    case TIME_WAIT:
      // TODO: timeout
      // client responds to all FINs from server with ACKs until timeout (2s)
      if (!check_conn(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0);
        break;
      }
      if (pkt->hdr.fin_f) {
        send_ack(rcv_nxt);
      }
      break;

    case CLOSED:
      // TODO: somehow inform the user that this connection has ended
      report(DROP, &pkt->hdr, 0, 0);
      break;
  }
}

bool CFP::send(PayloadT data) {
  struct cf_header tx_hdr = {};
  tx_hdr.seq = snd_nxt;
  return send_packet(&tx_hdr, data.first.data(), data.second);
}

void CFP::close() {
  // check for unacked packets
  // if so, wait for ACKs, if all have been ACKed, jump send FIN
  if (snd_una == snd_nxt) {
    send_fin();
    st = FIN_WAIT;
  } else {
    st = ACK_ALL;
  }
}

// TODO: retry on timeout
bool CFP::send_packet(struct cf_header* hdr, uint8_t* payload, size_t plsize) {
  struct cf_packet pkt = {};
  pkt.hdr = *hdr;
  pkt.hdr.conn = conn_id;
  memcpy(&pkt.payload, payload, plsize);

  // check no. of outstanding packets + the one we want to send
  if (!((snd_nxt - snd_una) + plsize <= cwnd)) {
    // must wait until the window has room
    return false;
  }

  una_buf.push_back(pkt);
  host_to_net(&pkt.hdr);
  mux.send(this, reinterpret_cast<uint8_t*>(&pkt), sizeof(struct cf_header) + plsize);
  report(SEND, hdr, cwnd, ssthresh);

  snd_nxt += plsize;

  return true;
}

void CFP::send_ack(uint32_t ack) {
  struct cf_packet pkt = {};

  pkt.hdr.ack_f = true;
  pkt.hdr.ack = ack;
  pkt.hdr.seq = snd_nxt;
  pkt.hdr.conn = conn_id;
  report(SEND, &pkt.hdr, cwnd, ssthresh);
  host_to_net(&pkt.hdr);

  // don't resend ACKs
  mux.send(this, reinterpret_cast<uint8_t*>(&pkt), sizeof(struct cf_header));
}

void CFP::send_fin() {
  struct cf_header tx_hdr = {};
  tx_hdr.fin_f = true;
  tx_hdr.seq = snd_nxt;
  tx_hdr.conn = conn_id;
  send_packet(&tx_hdr, nullptr, 0); // send FIN and retransmit if necessary
}

// client sends syn
void CFP::send_syn() {
  struct cf_header tx_hdr = {};

  snd_nxt = CLIENTISN + 1;
  snd_una = CLIENTISN + 1;

  // send syn
  tx_hdr.syn_f = true;
  tx_hdr.seq = CLIENTISN;
  send_packet(&tx_hdr, nullptr, 0);
}

// server recieves syn
// sends synack in response
void CFP::send_synack(struct cf_header* rx_hdr) { // ignore payload
  struct cf_header tx_hdr = {};

  if (!(rx_hdr->syn_f)) {
    // refuse the connection
    report(DROP, rx_hdr, 0, 0);
  }

  rcv_nxt = rx_hdr->seq + 1;
  snd_nxt = SERVERISN + 1;
  snd_una = SERVERISN + 1;

  // send syn-ack
  tx_hdr.syn_f = true;
  tx_hdr.ack_f = true;
  tx_hdr.seq = SERVERISN;
  tx_hdr.ack = rcv_nxt;
  tx_hdr.conn = conn_id;
  send_packet(&tx_hdr, nullptr, 0);
}

// client recieves the server's syn ack
// responds with an ACK packet with payload
void CFP::send_ack_payload(struct cf_header* rx_hdr) {
  struct cf_header tx_hdr = {};

  if (!(rx_hdr->syn_f && rx_hdr->ack_f && rx_hdr->ack == snd_nxt)) {
    // break;
    report(DROP, rx_hdr, 0, 0);
  }

  conn_id = rx_hdr->conn;
  rcv_nxt = rx_hdr->seq + 1;

  tx_hdr.ack_f = true;
  tx_hdr.ack = rcv_nxt;
  tx_hdr.seq = snd_nxt;
  tx_hdr.conn = conn_id;

  send_packet(&tx_hdr, first_payload.first.data(), first_payload.second);
}

bool CFP::check_conn(struct cf_header* rx_hdr) {
  return rx_hdr->conn == conn_id;
}

// peer has sent a packet, check it for ack
bool CFP::handle_ack(struct cf_header* rx_hdr) {
  if (rx_hdr->ack_f) {
    if (rx_hdr->ack > snd_una) {
      snd_una = rx_hdr->ack;
      clean_una_buf();
    }

    if (cwnd < ssthresh) {
      cwnd += PAYLOAD;
    } else if (cwnd >= ssthresh) {
      cwnd += PAYLOAD*PAYLOAD/cwnd;
    }

    return true;
  }
  return false;
}

void CFP::handle_payload(struct cf_packet* pkt, size_t pktsize) {
  if (pkt->hdr.seq != rcv_nxt) { // wasn't the packet we were expecting :/
    send_ack(rcv_nxt);
  } else if (pktsize > sizeof(struct cf_header)) { // received in order packet
    rcv_nxt += (pktsize - sizeof(struct cf_header));
    send_ack(rcv_nxt);
    ofile.write(reinterpret_cast<char*>(pkt->payload),
                pktsize - sizeof(struct cf_header));
  }
}

void CFP::handle_fin(struct cf_header* rx_hdr) {
  if (rx_hdr->fin_f) {
    rcv_nxt += 1;
    send_ack(rcv_nxt);

    // send fin
    struct cf_header tx_hdr = {};
    tx_hdr.fin_f = true;
    tx_hdr.seq = snd_nxt;
    send_packet(&tx_hdr, nullptr, 0);

    st = LAST_ACK;
  }
}

void CFP::clean_una_buf() {
  for (auto pkt = una_buf.begin(); pkt < una_buf.end(); pkt++) {
    if (pkt->hdr.seq < snd_una) {
      una_buf.erase(pkt);
    } else {
      break;
    }
  }
}
