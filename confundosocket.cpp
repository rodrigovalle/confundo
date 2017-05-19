#include "confundosocket.hpp"
#include "util.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

// return a copy of the header in network byte order
static struct cf_header host_to_net(struct cf_header* hdr) {
  struct cf_header net_hdr;

  net_hdr.seq = htonl(hdr->seq);
  net_hdr.ack = htonl(hdr->ack);
  net_hdr.conn = htons(hdr->conn);
  net_hdr.flgs = htons(hdr->flgs);

  return net_hdr;
}

// convert from network byte order in place
static void net_to_host(struct cf_header* hdr) {
  hdr->seq = ntohl(hdr->seq);
  hdr->ack = ntohl(hdr->ack);
  hdr->conn = ntohs(hdr->conn);
  hdr->flgs = ntohs(hdr->flgs);
}

ConfundoSocket::ConfundoSocket(const std::string& host, const std::string& port)
    : sock{host, port}, tx_hdr{} {
  connect();
}

ConfundoSocket::ConfundoSocket(const std::string& port) : sock{port}, tx_hdr{} {
  listen();
}

// TODO: currently blasts UDP packets @server, needs to wait
void ConfundoSocket::send_all(const std::string& data) {
  struct cf_packet pkt;
  pkt.hdr = tx_hdr;

  size_t i = 0;
  const uint8_t* buf = reinterpret_cast<const uint8_t*>(data.c_str());

  for (i = 0; i + PAYLOAD < data.size(); i += PAYLOAD) {
    send_packet(buf + i, PAYLOAD);
  }
  if (i < data.size()) {
    send_packet(buf + i, data.size() - i);
  }
}

void ConfundoSocket::send_header() {
  struct cf_header net_hdr = host_to_net(&tx_hdr);
  sock.send(reinterpret_cast<uint8_t*>(&net_hdr), sizeof(struct cf_header));
  report(SEND, &tx_hdr, 0, 0); // TODO: fill in cwnd and ssthresh
}

void ConfundoSocket::send_packet(const uint8_t data[], size_t size) {
  struct cf_packet pkt;

  pkt.hdr = host_to_net(&tx_hdr);
  memcpy(&pkt.payload, data, size);

  sock.send(reinterpret_cast<uint8_t*>(&pkt), size);
  report(SEND, &tx_hdr, 0, 0); // TODO: fill in cwnd and ssthresh
}

std::string ConfundoSocket::receive() { return ""; }

void ConfundoSocket::connect() {
  struct cf_header rx_hdr;

  // send syn
  tx_hdr.syn_f = true;
  tx_hdr.seq = CLIENTSYN;
  send_header();

  sock.recv(reinterpret_cast<uint8_t*>(&rx_hdr), sizeof(struct cf_header));
  net_to_host(&rx_hdr);
  report(RECV, &rx_hdr, 0, 0); // TODO: fill in cwnd and ssthresh

  if (!(rx_hdr.syn_f && rx_hdr.ack_f)) {
    throw std::runtime_error("no syn-ack response from server");
  }
}

void ConfundoSocket::listen() {
  struct cf_header rx_hdr;

  // ignore payload (like the linux kernel would)
  sock.recv_connect(reinterpret_cast<uint8_t*>(&rx_hdr), sizeof(struct cf_header));
  net_to_host(&rx_hdr);
  report(RECV, &rx_hdr, 0, 0); // TODO: fill in cwnd and ssthresh

  if (!rx_hdr.syn_f) {
    throw std::runtime_error("client sent misconfigured packet");
  }

  // send syn-ack
  tx_hdr.syn_f = true;
  tx_hdr.ack_f = true;
  tx_hdr.ack = rx_hdr.seq + 1;
  tx_hdr.seq = SERVERSYN;
  send_header();

  // TODO: client could send a header-only ACK or an ACK with a body
}
