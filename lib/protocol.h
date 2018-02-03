#ifndef protocol_h_INCLUDED
#define protocol_h_INCLUDED

// 2^15 bytes, 32 kb
#define PACKET_SIZE 0x8000

#define TRANSFER_START  1
#define TRANSFER_PACKET 2
#define TRANSFER_NEXT   3
#define TRANSFER_AGAIN  4
#define TRANSFER_END    5
#define TRANSFER_ERROR  6

#endif // protocol_h_INCLUDED

