#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h> // QUITAR
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define MAX_CONTS_PER_CLIENT 10

typedef struct CONT{
    char name[ 21 ];
    int status;
    struct sockaddr_in location;
} CONT;

typedef struct CONTS_DB{
    CONT containers[ MAX_CONTS_PER_CLIENT ];
} CONTS_DB;

typedef struct CLIENT{
    int desc;
    struct sockaddr_in addr;
    CONTS_DB db;
} CLIENT;

void CONT_define( CONT * this, char * name, struct sockaddr_in * location );

int CONT_get_status( CONT * this );

void CONTS_DB_init( CONTS_DB * this );

int CONTS_DB_alloc( CONTS_DB * this );

void CLIENT_define( CLIENT * this, int desc, struct sockaddr_in * addr );

int CLIENT_has_container( CLIENT * this, char * name );

int CLIENT_add_container( CLIENT * this, char * name, struct sockaddr_in * location );

void CLIENT_stop_container( CLIENT * this, int index );

void CLIENT_delete_container( CLIENT * this, int index );

void CLIENT_list_containers( CLIENT * this, char * listing );

#endif
