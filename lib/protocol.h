#ifndef protocol_h_INCLUDED
#define protocol_h_INCLUDED

// 2^15 bytes, 32 kb
#define PACKET_SIZE 0x8000

#define SUCCESS 0

#define START_DOWNLOAD 1
#define START_UPLOAD   2
#define REQUEST_PACKET   3
#define SEND_PACKET     4
#define CANCEL_UPLOAD   5
#define CANCEL_DOWNLOAD 6

#define ERROR_FILE_IO             1
#define ERROR_ALREADY_DOWNLOADING 2

struct reply {
    uint8_t status;
    uint16_t payloadLen;
    uint8_t *payload;
};

#define EMPTY_REPLY(s) (struct reply){s,0,NULL}

#endif // protocol_h_INCLUDED

