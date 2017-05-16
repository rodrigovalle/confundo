#include "confundosocket.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

static void clear_header(struct cf_packet* pkt) {
  memset(pkt, 0, sizeof(struct cf_header));
}

ConfundoSocket::ConfundoSocket(const std::string& host, const std::string& port)
    : sock{host, port} {
}

ConfundoSocket::ConfundoSocket(const std::string& port) : sock(port) {
  struct cf_header rx_hdr;
  struct cf_header tx_hdr = {};

  // ignore payload (like the linux kernel would)
  sock.recv(reinterpret_cast<uint8_t*>(&rx_hdr), sizeof(struct cf_header));
  if (!rx_hdr.syn_f) {
    throw std::runtime_error("client sent misconfigured packet");
  }

  // send syn-ack
  tx_hdr.syn_f = true;
  tx_hdr.ack_f = true;
  tx_hdr.ack = rx_hdr.seq + 1;
  tx_hdr.seq = SERVERSYN;
  sock.send(reinterpret_cast<uint8_t*>(&tx_hdr), sizeof(struct cf_header));
}

void ConfundoSocket::send_all(const std::string& data) {}

std::string ConfundoSocket::receive() { return ""; }

void ConfundoSocket::connect() {
  struct cf_header rx_hdr;
  struct cf_header tx_hdr = {};

  tx_hdr.syn_f = true;
  tx_hdr.seq = CLIENTSYN;

  // send syn
  sock.send(reinterpret_cast<uint8_t*>(&tx_hdr), sizeof(struct cf_header));
  sock.recv(reinterpret_cast<uint8_t*>(&rx_hdr), sizeof(struct cf_header));

  if (!(rx_hdr.syn_f && rx_hdr.ack_f)) {
    throw std::runtime_error("no syn-ack response from server");
  }

  memset(&tx_hdr, 0, sizeof(struct cf_header));
}

void ConfundoSocket::listen() {}
