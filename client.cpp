#include "udpsocket.hpp"

#include <string>
#include <thread>
#include <iostream>

int main()
{
  UDPSocket sock("localhost", 3000);
  sock.send // TODO
}
