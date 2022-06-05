#ifndef FORMATS_H
#define FORMATS_H

#define MAX_SIZE 1024
#define TYPES 1

typedef enum REQ_CODE{ REQ_NONE, REQ_CONNECT, REQ_PING, REQ_CREATE, REQ_STOP, REQ_DELETE, REQ_LIST, REQ_ACK, REQ_ERROR } REQ_CODE;

typedef struct REQ{
    REQ_CODE code;
    char data[ 240 ];
} REQ;

#endif
