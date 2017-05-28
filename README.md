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
[UDP reading partially from sockets](http://stackoverflow.com/questions/3069204)  
Linux man page for getaddrinfo()  
[Epoll Tutorial](https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/)  
[Timerfd Stackoverflow](https://stackoverflow.com/questions/12102740)  
[C++ reference](https://duckduckgo.com/?q=c%2B%2B+reference&ia=cheatsheet)
