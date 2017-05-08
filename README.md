# CS118 Project 2
Rodrigo Valle
104 494 120

## Wireshark dissector

For debugging purposes, you can use the wireshark dissector from `tcp.lua`.
The dissector requires at least version 1.12.6 of Wireshark with LUA support
enabled.

To enable the dissector for Wireshark session, use `-X` command line option,
specifying the full path to the `tcp.lua` script:

    wireshark -X lua_script:./confundo.lua

## Helpful links
[UDP tutorial](http://www.microhowto.info/howto/send_a_udp_datagram_in_c.html)  
Linux man page for getaddrinfo()
