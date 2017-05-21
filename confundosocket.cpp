#include "confundosocket.hpp"
#include "util.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

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

ConfundoSocket::ConfundoSocket(const std::string& host, const std::string& port)
    : sock{host, port} {
  // client: connect
  struct cf_header tx_hdr = {};
  struct cf_header rx_hdr;

  // send syn
  tx_hdr.syn_f = true;
  tx_hdr.seq = CLIENTSYN;
  send_header(&tx_hdr);

  snd_nxt = CLIENTSYN + 1;

  // recv syn-ack
  recv_header(&rx_hdr);
  if (!(rx_hdr.syn_f && rx_hdr.ack_f && rx_hdr.ack == snd_nxt)) {
    throw std::runtime_error("server did not respond with syn-ack");
  }

  conn_id = rx_hdr.conn;
  rcv_nxt = rx_hdr.seq + 1;

  // TODO: need to send an ACK with first payload to finish connecting
}

// TODO: conn_id
ConfundoSocket::ConfundoSocket(const std::string& port)
    : sock{port} {
  // server: listen
  struct cf_header tx_hdr = {};
  struct cf_header rx_hdr;

  // recv syn
  recv_header(&rx_hdr);
  if (!(rx_hdr.syn_f)) {
    throw std::runtime_error("received non-syn packet");
  }

  rcv_nxt = rx_hdr.seq + 1;

  // send syn-ack
  tx_hdr.syn_f = true;
  tx_hdr.ack_f = true;
  tx_hdr.seq = SERVERSYN;
  tx_hdr.ack = rcv_nxt;
  tx_hdr.conn = conn_id;
  send_header(&tx_hdr);

  snd_nxt = SERVERSYN + 1;

  // TODO: client could send an ACK with or without a payload
}

void ConfundoSocket::send_all(const std::string& data) {
  const uint8_t* buf = reinterpret_cast<const uint8_t*>(data.c_str());
  size_t i;

  for (i = 0; i + PAYLOAD < data.size(); i += PAYLOAD) {
    send_payload(buf + i, PAYLOAD);
  }
  if (i < data.size()) {
    send_payload(buf + i, data.size() - i);
  }
}

std::string ConfundoSocket::receive() {
  uint8_t payload[PAYLOAD];
  size_t size;

  size = recv_payload(payload, PAYLOAD);
  return std::string(reinterpret_cast<char*>(payload), size);
}

/* builds a new packet and sends it according to congestion control / reliable
 * delivery policy (ie. resend on timeouts) */
void ConfundoSocket::send_payload(const uint8_t data[], size_t size) {
  struct cf_packet tx_pkt = {};

  tx_pkt.hdr.seq = snd_nxt;
  tx_pkt.hdr.conn = conn_id;
  report(SEND, &tx_pkt.hdr, 0, 0); // TODO: fill in cwnd and ssthresh

  host_to_net(&tx_pkt.hdr);
  memcpy(&tx_pkt.payload, data, size);
  sock.send(reinterpret_cast<const uint8_t*>(&tx_pkt),
            sizeof(struct cf_header) + size);

  snd_nxt += size;
}

/* TODO: must retry on timeout
 * note: does not handle incrementing sequence variables */
inline void ConfundoSocket::send_header(const struct cf_header* hdr) {
  struct cf_header net_hdr = *hdr;
  host_to_net(&net_hdr);
  sock.send(reinterpret_cast<const uint8_t*>(&net_hdr),
            sizeof(struct cf_header));
  report(SEND, hdr, 0, 0);
}

/* note: does not handle incrementing sequence variables */
inline void ConfundoSocket::recv_header(struct cf_header* hdr) {
  sock.recv(reinterpret_cast<uint8_t*>(hdr), sizeof(struct cf_header));
  net_to_host(hdr);
  report(RECV, hdr, 0, 0); // TODO: fill in cwnd and ssthresh
}

/* recieves an in-order packet and acks, returning the payload */
size_t ConfundoSocket::recv_payload(uint8_t payload[], size_t size) {
  size_t nrecv, payloadlen;
  struct cf_header hdr = {};
  struct cf_packet rx_pkt;

  while (true) {
    nrecv = sock.recv(reinterpret_cast<uint8_t*>(&rx_pkt),
                      sizeof(struct cf_header) + size);
    net_to_host(&rx_pkt.hdr);
    report(RECV, &rx_pkt.hdr, 0, 0);

    // check correct order
    if (rx_pkt.hdr.seq != rcv_nxt) {
      continue;
    }

    payloadlen = nrecv - sizeof(struct cf_header);
    rcv_nxt += payloadlen;
    memcpy(payload, &rx_pkt.payload, payloadlen);

    // send ack
    hdr.ack_f = true;
    hdr.ack = rcv_nxt;
    send_header(&hdr);
    break;
  }

  return nrecv;
}
