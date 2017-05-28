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

/* Notes:
 * - when using mux.send() make sure to add the header size
 */

// server constructor
CFP::CFP(const UDPMux& udpmux, uint16_t id, const std::string& directory)
    : st{LISTEN}, snd_nxt{SERVERISN}, snd_una{SERVERISN}, rcv_nxt{0},
      cwnd{CWNDINIT}, ssthresh{SSTHRESHINIT}, conn_id{id}, mux{udpmux},
      ofile{directory + std::to_string(id) + ".file"}, directory{directory}
{}

// client constructor
CFP::CFP(const UDPMux& udpmux, PayloadT first_pl)
    : st{SYN_SENT}, snd_nxt{CLIENTISN}, snd_una{CLIENTISN}, rcv_nxt{0},
      cwnd{CWNDINIT}, ssthresh{SSTHRESHINIT},
      conn_id{0}, mux{udpmux}, first_payload{std::move(first_pl)}
{}

// move constructor
CFP::CFP(CFP&& o) noexcept
    : st{o.st}, snd_nxt{o.snd_nxt}, snd_una{o.snd_una}, rcv_nxt{o.rcv_nxt},
    una_buf{std::move(o.una_buf)}, cwnd{o.cwnd}, ssthresh{o.ssthresh},
    conn_id{o.conn_id}, mux{o.mux},
    first_payload{std::move(o.first_payload)}, ofile{std::move(o.ofile)}
{}

CFP::~CFP() {
  if (ofile.is_open()) {
    ofile.close();
  }
}

void CFP::recv_event(uint8_t data[], size_t size) {
  struct cf_packet* pkt = reinterpret_cast<struct cf_packet*>(data);
  net_to_host(&pkt->hdr);

  // reset disconnect timer to 10 seconds unless were waiting for connection close
  if (st != TIME_WAIT) {
    disconnect_timer.set_timeout(DISCONNECTTO);
  }

  switch (st) {
    case LISTEN:
      // server receives a SYN packet
      if (!(pkt->hdr.syn_f)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      st = SYN_RECEIVED;
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      handle_syn(&pkt->hdr);
      send_synack();
      break;

    case SYN_SENT:
      // client sent a SYN packet; expects a SYN+ACK from server
      if (!(pkt->hdr.syn_f && pkt->hdr.ack_f)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      st = ESTABLISHED;
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      handle_syn(&pkt->hdr);
      handle_ack(&pkt->hdr);
      conn_id = pkt->hdr.conn;

      send_ack_payload();
      break;

    case SYN_RECEIVED:
      // server has sent SYN+ACK, waiting for ACK from client
      if (!(check_conn(&pkt->hdr) && check_order(&pkt->hdr) && pkt->hdr.ack_f)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      st = ESTABLISHED;
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      handle_ack(&pkt->hdr);
      handle_payload(pkt, size);
      break;

    case ESTABLISHED:
      if (!check_conn(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      if (!check_order(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        resend_ack();
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);

      if (pkt->hdr.ack_f) {
        handle_ack(&pkt->hdr);
      }

      if (pkt->hdr.fin_f) {
        handle_fin();
        st = LAST_ACK;
        send_ack();
        send_fin();
        break; // FINs can't have payloads
      }

      handle_payload(pkt, size);
      break;

    case ACK_ALL:
      // client is waiting for server to ACK all packets before sending FIN
      if (!(check_conn(&pkt->hdr) && pkt->hdr.ack_f)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      handle_ack(&pkt->hdr);
      if (snd_una == snd_nxt) {
        st = FIN_WAIT;
        send_fin();
      }
      break;

    case FIN_WAIT:
      // client sent fin, expects ACK from server before starting 2s timeout
      if (!(check_conn(&pkt->hdr) && pkt->hdr.ack_f)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      handle_ack(&pkt->hdr);
      if (pkt->hdr.fin_f) {
        handle_fin();
        send_ack();
      }
      st = TIME_WAIT;
      disconnect_timer.set_timeout(FINWAITTIME);
      rto_timer.cancel_timeout();
      break;

    case LAST_ACK:
      // server sent FIN in response to client's FIN, expects ACK
      if (!(check_conn(&pkt->hdr) && pkt->hdr.ack_f)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      handle_ack(&pkt->hdr);
      st = CLOSED;
      terminate_gracefully();
      break;

    case TIME_WAIT:
      // client responds to all FINs from server with ACKs until timeout (2s)
      if (!(check_conn(&pkt->hdr))) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      if (pkt->hdr.fin_f) {
        handle_fin();
        send_ack();
      }
      break;

    case CLOSED:
      // can't send/receive any more data
      report(DROP, &pkt->hdr, 0, 0, false);
      break;
  }
}

void CFP::timeout_event() {
  rto_timer.read(); // clear timerfd for epoll()
  // didn't receive an ACK in RTO seconds
  // resend all packets XXX
  for (auto pkt : una_buf) {
    resend_packet(&pkt.first, pkt.second);
  }

  // reset timeout + congestion window
  ssthresh = cwnd/2;
  cwnd = CWNDINIT;
}

void CFP::disconnect_event() {
  disconnect_timer.read(); // clear timerfd
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
  disconnect_timer.set_timeout(DISCONNECTTO);
  st = SYN_SENT;
}

bool CFP::send(PayloadT data) {
  struct cf_header tx_hdr = {};
  if (st == ESTABLISHED) {
    return send_packet(&tx_hdr, data.first.data(), data.second);
  }
  return false;
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

bool CFP::send_packet(const struct cf_header* hdr, uint8_t* payload, size_t plsize) {
  struct cf_packet pkt = {};
  pkt.hdr = *hdr;
  pkt.hdr.seq = snd_nxt;
  pkt.hdr.conn = conn_id;
  if (payload) {
    memcpy(&pkt.payload, payload, plsize);
  }

  // check no. of outstanding packets + the one we want to send
  // note this can be rewritten as (snd_nxt - snd_una) + plsize <= cwnd
  // but we avoid this for wraparound reasons

  uint32_t winsize;
  if (snd_una <= snd_nxt) {
    winsize = snd_nxt - snd_una;
  } else {
    winsize = (MAXSEQ - snd_una) + snd_nxt - 1;
  }

  if (!(winsize + plsize <= cwnd)) {
    // must wait until the window has room
    return false;
  }

  una_buf.emplace_back(pkt, plsize);
  report(SEND, &pkt.hdr, cwnd, ssthresh, false);
  host_to_net(&pkt.hdr);
  mux.send(this, reinterpret_cast<uint8_t*>(&pkt), sizeof(struct cf_header) + plsize);

  snd_nxt = (snd_nxt + plsize) % MAXSEQ;
  rto_timer.set_timeout(RTO);

  return true;
}

void CFP::resend_packet(const struct cf_packet* pkt, size_t size) {
  struct cf_packet tx_pkt = *pkt;
  host_to_net(&tx_pkt.hdr);
  mux.send(this, reinterpret_cast<uint8_t*>(&tx_pkt), sizeof(struct cf_header) + size);
  report(SEND, &pkt->hdr, cwnd, ssthresh, true);
  rto_timer.set_timeout(RTO);
}

void CFP::send_ack() {
  struct cf_packet pkt = {};

  pkt.hdr.ack_f = true;
  pkt.hdr.ack = rcv_nxt;
  pkt.hdr.seq = snd_nxt;
  pkt.hdr.conn = conn_id;
  report(SEND, &pkt.hdr, cwnd, ssthresh, false);

  // don't add ACKs to una_buf
  host_to_net(&pkt.hdr);
  mux.send(this, reinterpret_cast<uint8_t*>(&pkt), sizeof(struct cf_header));
  rto_timer.set_timeout(RTO);
}

void CFP::resend_ack() {
  struct cf_packet pkt = {};

  pkt.hdr.ack_f = true;
  pkt.hdr.ack = rcv_nxt;
  pkt.hdr.seq = snd_nxt;
  pkt.hdr.conn = conn_id;
  report(SEND, &pkt.hdr, cwnd, ssthresh, true);

  // don't add ACKs to una_buf
  host_to_net(&pkt.hdr);
  mux.send(this, reinterpret_cast<uint8_t*>(&pkt), sizeof(struct cf_header));
  rto_timer.set_timeout(RTO);
}

void CFP::send_fin() {
  struct cf_header tx_hdr = {};
  tx_hdr.fin_f = true;
  send_packet(&tx_hdr, nullptr, 0); // send FIN and retransmit if necessary
  
  // fin takes 1 byte in the stream
  snd_nxt += 1;
  snd_nxt %= MAXSEQ;
}

// client sends syn
void CFP::send_syn() {
  struct cf_header tx_hdr = {};

  // create syn packet & send
  tx_hdr.syn_f = true;
  send_packet(&tx_hdr, nullptr, 0);

  // syn takes 1 byte in the stream
  snd_nxt += 1;
  snd_nxt %= MAXSEQ;
}

// server recieves syn
// sends synack in response
void CFP::send_synack() { // ignore payload
  struct cf_header tx_hdr = {};

  // create syn-ack & send
  tx_hdr.syn_f = true;
  tx_hdr.ack_f = true;
  tx_hdr.ack = rcv_nxt;
  send_packet(&tx_hdr, nullptr, 0);

  // syn takes 1 byte in the stream
  snd_nxt += 1;
  snd_nxt %= MAXSEQ;
}

// client recieves the server's syn ack
// responds with an ACK packet with payload
void CFP::send_ack_payload() {
  struct cf_header tx_hdr = {};

  tx_hdr.ack_f = true;
  tx_hdr.ack = rcv_nxt;
  send_packet(&tx_hdr, first_payload.first.data(), first_payload.second);
}

bool CFP::check_conn(struct cf_header* rx_hdr) {
  return rx_hdr->conn == conn_id;
}

bool CFP::check_order(struct cf_header* rx_hdr) {
  return rx_hdr->seq == rcv_nxt;
}

// peer has sent a packet, check it for ack
void CFP::handle_ack(struct cf_header* rx_hdr) {

  /* 
   * check cases for acceptable ACK (SND.UNA < SEG.ACK =< SND.NXT):
   *  - common (snd_una < snd_nxt) and
   *    +-----------------------------+
   *    |  |XX|XX|XX|XX|XX|XX|X |  |  |
   *    +-----------------------------+
   *        ^                  ^
   *     snd_una            snd_nxt
   *
   *  - wraparound (snd_nxt < snd_una)
   *    +-----------------------------+
   *    |XX|  |  |  |  |  |  | X|XX|XX|
   *    +-----------------------------+
   *        ^                  ^
   *     snd_nxt            snd_una
   */
  if (((snd_una < snd_nxt) && (snd_una < rx_hdr->ack && rx_hdr->ack <= snd_nxt)) ||
      ((snd_nxt < snd_una) && (snd_una < rx_hdr->ack || rx_hdr->ack <= snd_nxt)) ||
       (rx_hdr->ack == snd_nxt)) {
    // update the unacknowedged packets window
    snd_una = rx_hdr->ack;
    clean_una_buf();

    // update the congestion control window
    if (cwnd < ssthresh) {
      cwnd += PAYLOAD;
    } else {
      cwnd += PAYLOAD*PAYLOAD/cwnd;
      cwnd = (cwnd > CWNDCAP) ? CWNDCAP : cwnd;
    }
  }
}

void CFP::handle_syn(struct cf_header* rx_hdr) {
  rcv_nxt = rx_hdr->seq + 1;
  rcv_nxt %= MAXSEQ;
}

void CFP::handle_fin() {
  rcv_nxt += 1;
  rcv_nxt %= MAXSEQ;
}

// returns false if out of order (dropped) packet
void CFP::handle_payload(struct cf_packet* pkt, size_t pktsize) {
  if (pktsize > sizeof(struct cf_header)) {
    rcv_nxt += (pktsize - sizeof(struct cf_header));
    rcv_nxt %= MAXSEQ;
    send_ack();
    ofile.write(reinterpret_cast<char*>(pkt->payload),
                pktsize - sizeof(struct cf_header));
  }
}

void CFP::clean_una_buf() {
  size_t plsize;
  struct cf_packet pkt;

  for (auto i = una_buf.begin(); i < una_buf.end(); i++) {
    std::tie(pkt, plsize) = *i;

    // calculate the ack we expect for this packet
    uint32_t exp_ack = (pkt.hdr.seq + plsize) % MAXSEQ;

    // Segment is fully acked if seq + length <= current_ack
    // In our case, snd_una == current_ack
    // three cases:
    //  - snd_una < snd_nxt: no wraparound
    //  - snd_nxt < snd_una: window is wrapping
    //  - snd_nx == snd_una: window is empty, everything is ACKed
    if (((snd_una < snd_nxt) && (exp_ack <= snd_una || exp_ack > snd_nxt)) ||
        ((snd_nxt < snd_una) && (exp_ack <= snd_una && exp_ack > snd_nxt)) ||
        ((snd_nxt == snd_una))) {
      una_buf.erase(i);
    } else {
      break; // these packets are ordered by sequence number
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
    ofile << "ERROR";
  }
  throw connection_closed_ungracefully{*this, why.c_str()};
}
