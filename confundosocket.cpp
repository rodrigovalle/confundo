#include "confundosocket.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

enum Operation {
  SEND,
  RECV,
  DROP
};

static void clear_header(struct cf_packet* pkt) {
  memset(pkt, 0, sizeof(struct cf_header));
}

static void report(Operation op, struct cf_header* hdr, uint32_t cwnd,
                   uint32_t ssthresh) {
  switch (op) {
    case SEND:
      std::cout << "SEND ";
    case RECV:
      std::cout << "RECV ";
    case DROP:
      std::cout << "DROP ";
  }

  std::cout << hdr->seq << " ";
  std::cout << hdr->ack << " ";
  std::cout << hdr->conn << " ";
  std::cout << cwnd << " ";
  std::cout << ssthresh;

  if (hdr->ack_f) {
    std::cout << " ACK";
  }
  if (hdr->syn_f) {
    std::cout << " SYN";
  }
  if (hdr->fin_f) {
    std::cout << " FIN";
  }
  std::cout << std::endl;
}

ConfundoSocket::ConfundoSocket(const std::string& host, const std::string& port)
    : sock{host, port} {
}

ConfundoSocket::ConfundoSocket(const std::string& port) : sock(port) {
  struct cf_header rx_hdr;
  struct cf_header tx_hdr = {};

  // ignore payload (like the linux kernel would)
  sock.recv(reinterpret_cast<uint8_t*>(&rx_hdr), sizeof(struct cf_header));
  report(RECV, &rx_hdr, 0, 0); // TODO: fill in cwnd and ssthresh

  if (!rx_hdr.syn_f) {
    throw std::runtime_error("client sent misconfigured packet");
  }

  // send syn-ack
  tx_hdr.syn_f = true;
  tx_hdr.ack_f = true;
  tx_hdr.ack = rx_hdr.seq + 1;
  tx_hdr.seq = SERVERSYN;

  sock.send(reinterpret_cast<uint8_t*>(&tx_hdr), sizeof(struct cf_header));
  report(SEND, &tx_hdr, 0, 0); // TODO: fill in cwnd and ssthresh
}

// TODO: currently blasts UDP packets @server, needs to wait
void ConfundoSocket::send_all(const std::string& data) {
}

std::string ConfundoSocket::receive() { return ""; }

void ConfundoSocket::connect() {
  struct cf_header rx_hdr;
  struct cf_header tx_hdr = {};

  tx_hdr.syn_f = true;
  tx_hdr.seq = CLIENTSYN;

  // send syn
  sock.send(reinterpret_cast<uint8_t*>(&tx_hdr), sizeof(struct cf_header));
  report(SEND, &tx_hdr, 0, 0); // TODO: fill in cwnd and ssthresh

  sock.recv(reinterpret_cast<uint8_t*>(&rx_hdr), sizeof(struct cf_header));
  report(RECV, &rx_hdr, 0, 0); // TODO: fill in cwnd and ssthresh

  if (!(rx_hdr.syn_f && rx_hdr.ack_f)) {
    throw std::runtime_error("no syn-ack response from server");
  }

  memset(&tx_hdr, 0, sizeof(struct cf_header));
}

void ConfundoSocket::listen() {}
