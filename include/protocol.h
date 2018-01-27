#ifndef protocol_h_INCLUDED
#define protocol_h_INCLUDED

enum TransferCommands {
    TRANSFER_START = 1,
    TRANSFER_PACKET,
    TRANSFER_NEXT,
    TRANSFER_AGAIN,
    TRANSFER_END,
    TRANSFER_ERROR
};

#endif // protocol_h_INCLUDED

