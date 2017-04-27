# Protocol Specification

The payload of each UDP packet sent by the server MUST start with the
follwing 12-byte header:


    |<----- 0 ----->|<----- 1 ----->|<----- 2 ----->|<----- 3 ----->|
     0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        Sequence Number                        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     Acknowledgment Number                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |         Connection ID         |         Not Used        |A|S|F|
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


    ``` C++
    #include <cstdint>

    struct header {
      uint32_t seq;      /* sequence number              */
      uint32_t ack;      /* acknowledgement number       */
      uint16_t conn;     /* connection id                */
      uint16_t xxx: 13;  /* (unused, should be zero)     */
      uint16_t ACK: 1;   /* acknowledge packet received  */
      uint16_t SYN: 1;   /* synchronize sequence numbers */
      uint16_t FIN: 1;   /* no more data from sender     */
    };
    ```

[C++ Bitfields](http://stackoverflow.com/questions/31726191)

Where
  - `Sequence Number`: The sequence number of the first data octet in this
     packet (except when SYN is present). If SYN is present the sequence number
     is the initial sequence number (ISN) and the first data octet is ISN+1.

     The sequence number is given in the unit of **bytes**.

  - `Acknowledgement Number`: If the ACK control bit is set, this field
     contains the value of the next sequence number the sender of the segment
     is expecting to receive. Once a connection is established this is always
     sent. If ACK is not set, this field should be zero.

     The acknowledgement number is given in the unit of **bytes**.

  - `Connection ID`: A number representing the connection identifier.

  - `Not Used`: Must be zero.

  - `A`: Indicates that the value of the `Acknowledgement Number` field is
     valid.

  - `S`: Synchronize sequence numbers (Confundo connection establishment)

  - `F`: No more data from sender (Confundo connection termination)


## Requirements

  - The maximum UDP packet size is 524 bytes, including a header (maximum of
    512 bytes in the payload).

  - The maximum sequence and acknowledgement number should be 102400 and be
    reset to zero whenever it reaches the maximum value.

  - Packet retransmission (and appropriate congestion control actions) should
    be triggered when no data was acknowledged for more than 0.5 seconds
    (fixed retransmission timeout).

  - Initial and minimum congestion window size (`CWND`) should be 512.

  - Initial slow-start threshold (`SS-THRESH`) should be 10000.

  - If `ACK` field is not set, `Acknowledgment Number` field should be set to
    0.

  - `FIN` should take logically one byte of the data stream (same as in TCP,
    see examples).

  - `FIN` and `FIN | ACK` packets must not carry any payload.
