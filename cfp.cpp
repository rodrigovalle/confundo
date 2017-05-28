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
      cwnd{CWNDINIT}, ssthresh{SSTHRESHINIT}, mux{udpmux},
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
      // server receieves a SYN packet
      if (!handle_syn(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      st = SYN_RECEIVED;
      send_synack(&pkt->hdr);
      break;

    case SYN_SENT:
      // client sent a SYN packet; expects a SYN+ACK from server
      if (!(handle_syn(&pkt->hdr) && handle_ack(&pkt->hdr))) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      st = ESTABLISHED;
      conn_id = pkt->hdr.conn;

      send_ack_payload(&pkt->hdr);
      break;

    case SYN_RECEIVED:
      // server has received the initial SYN, waiting for ACK
      if (!(handle_ack(&pkt->hdr) && check_conn(&pkt->hdr))) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      st = ESTABLISHED;
      if (!handle_payload(pkt, size)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      break;

    case ESTABLISHED:
      if (!check_conn(&pkt->hdr)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      handle_ack(&pkt->hdr);
      if (handle_fin(&pkt->hdr)) {
        st = LAST_ACK;
        break; // FINs can't have payloads
      }
      if (!handle_payload(pkt, size)) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      break;

    case ACK_ALL:
      // client is waiting for server to ACK all packets before sending FIN
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      if (!(check_conn(&pkt->hdr) && handle_ack(&pkt->hdr))) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      if (snd_una == snd_nxt) {
        st = FIN_WAIT;
        send_fin();
      }
      break;

    case FIN_WAIT:
      // client sent fin, expects ACK from server before starting 2s timeout
      if (!(check_conn(&pkt->hdr) && handle_ack(&pkt->hdr))) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      if (handle_fin(&pkt->hdr)) {
        send_ack(rcv_nxt);
      }
      st = TIME_WAIT;
      disconnect_timer.set_timeout(FINWAITTIME);
      rto_timer.set_timeout(0);
      break;

    case LAST_ACK:
      // server sent FIN in response to client's FIN, expects ACK
      if (!(check_conn(&pkt->hdr) && handle_ack(&pkt->hdr))) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      st = CLOSED;
      terminate_gracefully();
      break;

    case TIME_WAIT:
      // client responds to all FINs from server with ACKs until timeout (2s)
      if (!check_conn(&pkt->hdr) || !pkt->hdr.fin_f) {
        report(DROP, &pkt->hdr, 0, 0, false);
        break;
      }
      report(RECV, &pkt->hdr, cwnd, ssthresh, false);
      send_ack(rcv_nxt);
      break;

    case CLOSED:
      // can't send/receive any more data
      report(DROP, &pkt->hdr, 0, 0, false);
      break;
  }
}

void CFP::timeout_event() {
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

bool CFP::send_packet(const struct cf_header* hdr, uint8_t* payload, size_t plsize) {
  struct cf_packet pkt = {};
  pkt.hdr = *hdr;
  pkt.hdr.conn = conn_id;
  memcpy(&pkt.payload, payload, plsize);

  // check no. of outstanding packets + the one we want to send
  // note this can be rewritten as (snd_nxt - snd_una) + plsize <= cwnd
  // but we avoid this for wraparound reasons

  uint32_t winsize;
  if (snd_una <= snd_nxt) {
    winsize = snd_nxt - snd_una;
  } else {
    winsize = (MAXSEQ - snd_una) + snd_nxt;
  }

  switch (st) {
    case LISTEN:
      std::cerr << "LISTEN";
      break;
    case SYN_SENT:
      std::cerr << "SYN_SENT";
      break;
    case SYN_RECEIVED:
      std::cerr << "SYN_RECEIVED";
      break;
    case ESTABLISHED:
      std::cerr << "ESTABLISHED";
      break;
    case ACK_ALL:
      std::cerr << "ACK_ALL";
      break;
    case FIN_WAIT:
      std::cerr << "FIN_WAIT";
      break;
    case LAST_ACK:
      std::cerr << "LAST_ACK";
      break;
    case TIME_WAIT:
      std::cerr << "TIME_WAIT";
      break;
    case CLOSED:
      std::cerr << "CLOSED";
      break;
    default:
      std::cerr << "whoops";
      break;

  }
  std::cerr << std::endl;

  if (!(winsize + plsize <= cwnd)) {
    // must wait until the window has room
    std::cerr << "can't send any more packets:" << std::endl;
    std::cerr << "  snd_una: " << snd_una << std::endl;
    std::cerr << "  snd_nxt: " << snd_nxt << std::endl;
    std::cerr << "  plsize:  " << plsize << std::endl;
    std::cerr << "  cwnd:    " << cwnd << std::endl;
    return false;
  }

  una_buf.emplace_back(pkt, plsize);
  host_to_net(&pkt.hdr);
  mux.send(this, reinterpret_cast<uint8_t*>(&pkt), sizeof(struct cf_header) + plsize);
  report(SEND, hdr, cwnd, ssthresh, false);

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

void CFP::send_ack(uint32_t ack) {
  struct cf_packet pkt = {};

  pkt.hdr.ack_f = true;
  pkt.hdr.ack = ack;
  pkt.hdr.seq = snd_nxt;
  pkt.hdr.conn = conn_id;
  report(SEND, &pkt.hdr, cwnd, ssthresh, false);

  // don't add ACKs to una_buf
  host_to_net(&pkt.hdr);
  mux.send(this, reinterpret_cast<uint8_t*>(&pkt), sizeof(struct cf_header));
  rto_timer.set_timeout(RTO);
}

void CFP::resend_ack(uint32_t ack) {
  struct cf_packet pkt = {};

  pkt.hdr.ack_f = true;
  pkt.hdr.ack = ack;
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
  tx_hdr.seq = snd_nxt;
  tx_hdr.conn = conn_id;
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
  tx_hdr.seq = snd_nxt;
  send_packet(&tx_hdr, nullptr, 0);

  // syn takes 1 byte in the stream
  snd_nxt += 1;
  snd_nxt %= MAXSEQ;
}

// server recieves syn
// sends synack in response
void CFP::send_synack(struct cf_header* rx_hdr) { // ignore payload
  struct cf_header tx_hdr = {};

  // create syn-ack & send
  tx_hdr.syn_f = true;
  tx_hdr.ack_f = true;
  tx_hdr.seq = snd_nxt;
  tx_hdr.ack = rcv_nxt;
  tx_hdr.conn = conn_id;
  send_packet(&tx_hdr, nullptr, 0);

  // syn takes 1 byte in the stream
  snd_nxt += 1;
  snd_nxt %= MAXSEQ;
}

// client recieves the server's syn ack
// responds with an ACK packet with payload
void CFP::send_ack_payload(struct cf_header* rx_hdr) {
  struct cf_header tx_hdr = {};

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
  if (!(rx_hdr->ack_f)) {
    return false;
  }

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
    std::cerr << "SND.UNA: " << snd_una << std::endl;
    std::cerr << "SND.NXT: " << snd_nxt << std::endl;
    clean_una_buf();

    // update the congestion control window
    if (cwnd < ssthresh) {
      cwnd += PAYLOAD;
    } else {
      cwnd += PAYLOAD*PAYLOAD/cwnd;
    }
  }
  return true;
}

bool CFP::handle_syn(struct cf_header* rx_hdr) {
  if (!(rx_hdr->syn_f)) {
    return false;
  }

  rcv_nxt = rx_hdr->seq + 1;
  return true;
}

bool CFP::handle_fin(struct cf_header* rx_hdr) {
  if (!rx_hdr->fin_f) {
    return false;
  }
  rcv_nxt += 1;
  return true;
}

// returns false if out of order (dropped) packet
bool CFP::handle_payload(struct cf_packet* pkt, size_t pktsize) {
  if (pkt->hdr.seq != rcv_nxt) {
    resend_ack(rcv_nxt);
    return false; // out of order packet

  } else if (pktsize > sizeof(struct cf_header)) { // received in order packet
    rcv_nxt += (pktsize - sizeof(struct cf_header));
    send_ack(rcv_nxt);
    ofile.write(reinterpret_cast<char*>(pkt->payload),
                pktsize - sizeof(struct cf_header));
    return true;
  }
  return true; // packet was in order but had no payload
}

void CFP::clean_una_buf() {
  size_t plsize;
  struct cf_packet pkt;
  std::cerr << "===========UNA BUF============" << std::endl;

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
      //break, these packets are ordered by sequence number
      std::cerr << pkt.hdr.seq << "+" << plsize << std::endl;
    }
  }
  std::cerr << "==============================" << std::endl;
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
