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

// server constructor
CFP::CFP(const UDPMux& udpmux, uint16_t id, const std::string& directory)
    : st{LISTEN}, conn_id{id}, cwnd{CWNDINIT}, ssthresh{SSTHRESHINIT},
      mux{udpmux}, ofile{directory + std::to_string(id) + ".file"},
      directory{directory} {}

// client constructor
CFP::CFP(const UDPMux& udpmux, PayloadT first_pl)
    : st{SYN_SENT}, conn_id{0}, cwnd{CWNDINIT}, ssthresh{SSTHRESHINIT},
      mux{udpmux}, first_payload{first_pl} {}

CFP::CFP(CFP&& o) : st{o.st}, snd_nxt{o.snd_nxt}, rcv_nxt{o.rcv_nxt},
    conn_id{o.conn_id}, una_buf{o.una_buf}, cwnd{o.cwnd}, ssthresh{o.ssthresh},
    mux{o.mux}, ofile{std::move(o.ofile)}, first_payload{o.first_payload} {}

CFP::~CFP() {
  if (ofile.is_open()) {
    ofile.close();
  }
}

void CFP::recv_event(uint8_t data[], size_t size) {
  struct cf_packet* pkt = reinterpret_cast<struct cf_packet*>(data);
  net_to_host(&pkt->hdr);
  report(RECV, &pkt->hdr, cwnd, ssthresh, false);

  // reset disconnect timer to 10 seconds
  disconnect_timer.set_timeout(DISCONNECTTO);

  switch (st) {
    case LISTEN:
      std::cerr << "LISTEN" << std::endl; // XXX: for debugging only
      if (!pkt->hdr.syn_f) {
        report(DROP, &pkt->hdr, 0, 0, false);
      }
      send_synack(&pkt->hdr);
      st = SYN_RECEIVED;
      break;

    case SYN_SENT:
      if (!(pkt->hdr.syn_f && pkt->hdr.ack_f && pkt->hdr.ack == snd_nxt)) {
        report(DROP, &pkt->hdr, 0, 0, false);
      }

      std::cerr << "SYN_SENT" << std::endl; // XXX: for debugging only
      send_ack_payload(&pkt->hdr);
      st = ESTABLISHED;
      break;

    case SYN_RECEIVED:
      std::cerr << "SYN_RECEIVED" << std::endl; // XXX: for debugging only
      if (!handle_ack(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      if (!check_conn(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      st = ESTABLISHED;
      handle_payload(pkt, size);
      break;

    case ESTABLISHED:
      std::cerr << "ESTABLISHED" << std::endl; // XXX: for debugging only
      if (!check_conn(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      handle_ack(&pkt->hdr);
      handle_payload(pkt, size);
      handle_fin(&pkt->hdr);
      break;

    case ACK_ALL:
      std::cerr << "ACK_ALL" << std::endl; // XXX: for debugging only
      handle_ack(&pkt->hdr);
      if (snd_una == snd_nxt) {
        send_fin();
        st = FIN_WAIT;
      }
      break;

    case FIN_WAIT:
      std::cerr << "FIN_WAIT" << std::endl; // XXX: for debugging only
      // client sent fin, expects ACK
      if (!handle_ack(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      st = TIME_WAIT;
      disconnect_timer.set_timeout(FINWAITTIME);
      break;

    case LAST_ACK:
      std::cerr << "LAST_ACK" << std::endl; // XXX: for debugging only
      // server sent fin, expects ACK
      if (!handle_ack(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      st = CLOSED;
      terminate_gracefully();
      break;

    case TIME_WAIT:
      std::cerr << "TIME_WAIT" << std::endl; // XXX: for debugging only
      // client responds to all FINs from server with ACKs until timeout (2s)
      if (!check_conn(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      if (pkt->hdr.fin_f) {
        send_ack(rcv_nxt);
      }
      break;

    case CLOSED:
      std::cerr << "CLOSED" << std::endl; // XXX: for debugging only
      report(DROP, &pkt->hdr, 0, 0, false);
      break;
  }
}

void CFP::timeout_event() {
  // resend all packets
  for (auto pkt : una_buf) {
    send_packet_nocc(&pkt.first, pkt.second, true);
  }
  // reset timeout
  rto_timer.set_timeout(RTO);
}

void CFP::disconnect_event() {
  switch (st) {
    case TIME_WAIT:
      st = CLOSED;
      terminate_gracefully();
      break;

    default:
      terminate_ungracefully("CFP connection timed out (10s)");
      break;
  }
}

void CFP::start() {
  send_syn();
}

bool CFP::send(PayloadT data) {
  struct cf_header tx_hdr = {};
  tx_hdr.seq = snd_nxt;
  if (st == ESTABLISHED) {
    return send_packet(&tx_hdr, data.first.data(), data.second);
  } else {
    return false;
  }
}

void CFP::close() {
  // check for unacked packets
  // if so, wait for ACKs, if all have been ACKed, send FIN immediately
  if (snd_una == snd_nxt) {
    send_fin();
    st = FIN_WAIT;
  } else {
    st = ACK_ALL;
  }
}

// TODO: retry on timeout
bool CFP::send_packet(const struct cf_header* hdr, uint8_t* payload, size_t plsize) {
  struct cf_packet pkt = {};
  pkt.hdr = *hdr;
  pkt.hdr.conn = conn_id;
  memcpy(&pkt.payload, payload, plsize);

  // check no. of outstanding packets + the one we want to send
  if (!((snd_nxt - snd_una) + plsize <= cwnd)) {
    // must wait until the window has room
    return false;
  }

  una_buf.emplace_back(pkt, sizeof(struct cf_header) + plsize);
  host_to_net(&pkt.hdr);
  mux.send(this, reinterpret_cast<uint8_t*>(&pkt), sizeof(struct cf_header) + plsize);
  report(SEND, hdr, cwnd, ssthresh, false);

  snd_nxt += plsize;

  return true;
}

void CFP::send_packet_nocc(const struct cf_packet* pkt, size_t size, bool resnd) {
  struct cf_packet tx_pkt = *pkt;
  host_to_net(&tx_pkt.hdr);
  mux.send(this, reinterpret_cast<uint8_t*>(&tx_pkt), size);
  report(SEND, &pkt->hdr, cwnd, ssthresh, resnd);
}

void CFP::send_ack(uint32_t ack) {
  struct cf_packet pkt = {};

  pkt.hdr.ack_f = true;
  pkt.hdr.ack = ack;
  pkt.hdr.seq = snd_nxt;
  pkt.hdr.conn = conn_id;
  report(SEND, &pkt.hdr, cwnd, ssthresh, false);

  // don't resend ACKs
  host_to_net(&pkt.hdr);
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
  struct cf_packet tx_pkt = {};

  snd_nxt = CLIENTISN + 1;
  snd_una = CLIENTISN + 1;

  // create syn packet
  tx_pkt.hdr.syn_f = true; // XXX
  tx_pkt.hdr.seq = CLIENTISN;

  // resend if necessary
  una_buf.emplace_back(tx_pkt, sizeof(cf_header));
  rto_timer.set_timeout(RTO);

  send_packet_nocc(&tx_pkt, sizeof(cf_header), false);
}

// server recieves syn
// sends synack in response
void CFP::send_synack(struct cf_header* rx_hdr) { // ignore payload
  struct cf_packet tx_pkt = {};

  rcv_nxt = rx_hdr->seq + 1;
  snd_nxt = SERVERISN + 1;
  snd_una = SERVERISN + 1;

  // create syn-ack
  tx_pkt.hdr.syn_f = true; // XXX
  tx_pkt.hdr.ack_f = true;
  tx_pkt.hdr.ack_f = true;
  tx_pkt.hdr.seq = SERVERISN;
  tx_pkt.hdr.ack = rcv_nxt;
  tx_pkt.hdr.conn = conn_id;

  // resend if necessary
  una_buf.emplace_back(tx_pkt, sizeof(cf_header));
  rto_timer.set_timeout(RTO);
  disconnect_timer.set_timeout(DISCONNECTTO);

  send_packet_nocc(&tx_pkt, sizeof(cf_header), false);
}

// client recieves the server's syn ack
// responds with an ACK packet with payload
void CFP::send_ack_payload(struct cf_header* rx_hdr) {
  struct cf_header tx_hdr = {};

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

    rto_timer.set_timeout(RTO);

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
    if (pkt->first.hdr.seq < snd_una) {
      una_buf.erase(pkt);
    } else {
      break;
    }
  }
}

void CFP::terminate_gracefully() {
  throw connection_closed_gracefully{*this};
}

void CFP::terminate_ungracefully(const std::string& why) {
  if (ofile.is_open()) {
    ofile.close();
    ofile.open(directory + std::to_string(conn_id) + ".file");
    ofile << "ERROR: " << why << std::endl;
  }
  throw connection_closed_ungracefully{*this, why.c_str()};
}
