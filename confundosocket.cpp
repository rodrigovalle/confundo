#include "confundosocket.hpp"
#include <string>

ConfundoSocket::ConfundoSocket(const std::string& host, const std::string& port)
    : sock(host, port) {}

ConfundoSocket::ConfundoSocket(const std::string& port) : sock(port) {}

void ConfundoSocket::send_all(const std::string& data) {}

std::string ConfundoSocket::receive() { return ""; }

void ConfundoSocket::connect() {}

void ConfundoSocket::listen() {}
