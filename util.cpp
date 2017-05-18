#include "util.hpp"

void report(op_T op, struct cf_header* hdr, uint32_t cwnd, uint32_t ssthresh) {
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