#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#include "../lib/CLIENT/client.h"
#include "../lib/AGENT/agent.h"
#include "../lib/FORMATS/formats.h"

/* SE DEFINEN LOS PUERTOS */
#define AC_PORT 8000
#define SH_PORT 9000

/* ESTRUCTURA SERVIDOR PARA FACILITAR EL ORDEN */
typedef struct SERVER{
    int desc;
    struct sockaddr_in addr;
} SERVER;

/* SE DECLARA GLOBALMENTE UN APUNTADOR A UNA VARIABLE TIPO AGENTS_DB (BASE DE DATOS DE AGENTES). ESTE APUNTADOR SERA RELACIONADO */
/* MAS TARDE CON EL SEGMENTO DE MEMORIA COMPARTIDA */
AGENTS_DB * db;

/* ESTA FUNCION INICIALIZA UN SERVIDOR TCP EN EL PUERTO port. LA FUNCION RETORNA -1 SE SE PRESENTA UN ERROR */
int initialize_tcp_server( SERVER * server, int port ){
    server->desc = socket( AF_INET, SOCK_STREAM, 0 );
    if( server->desc == -1 ){
        return -1;
    }
    server->addr.sin_family = AF_INET;
    server->addr.sin_addr.s_addr = INADDR_ANY;
    server->addr.sin_port = htons( port );
    if( bind( server->desc, ( struct sockaddr * ) &server->addr, sizeof( struct sockaddr_in ) ) == -1 ) {
        return -1;
    }
    if( listen( server->desc, 3 ) == -1 ){ // DEBERIA ESTANDARIZARSE
        close( server->desc );
        server->desc = -1;
        return -1;
    }
    return server->desc;
}

/* ESTA FUNCION HACE POLLING SOBRE UN SOCKET PARA VER SI EN UN TIEMPO MENOR A TIMEOUT (EN MILISEGUNDOS) SE PUEE EJECUTAR ALGUNA OPERACION DE I/O */
/* SOBRE EL SOCKET */
int socket_timeout( int desc, int ms ){
    struct pollfd target;
    target.fd = desc;
    target.events = POLLIN;
    return poll( &target, 1, ms );
}

/* ESTA FUNCION SE ENCARGA DE ATENDER LAS SOLICITUDES DEL CLIENTE ECS */
void attend( CLIENT * client, REQ * req ){
    REQ res;
    int aux;
    int cont_index;
    int agent_index;
    switch( req->code ){
        case REQ_CREATE:
            if( CONTS_DB_alloc( &client->db ) == -1 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: client has no capacity...\n" );
                break;
            }
            cont_index = CLIENT_has_container( client, req->data );
            if( cont_index > -1 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: container name is already in use...\n" );
                break;
            }
            agent_index = AGENTS_DB_select( db );
            if( agent_index == -1 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: there are no agents capable of attending request...\n" );
                break;
            }
            if( !AGENT_is_connected( &db->agents[ agent_index ] ) ){
                AGENT_connect( &db->agents[ agent_index ] );
            }
            if( !AGENT_is_connected( &db->agents[ agent_index ] ) ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: could not stablish connection to agent...\n" );
                break;
            }
            send( db->agents[ agent_index ].desc, req, sizeof( REQ ), 0 );
            int aux = socket_timeout( db->agents[ agent_index ].desc, TIMEOUT );
            if( aux == -1 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: request may have not been attended...\n" );
                break;
            }
            if( aux == 0 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: agent response timedout, request may have not been attended...\n" );
                break;
            }
            aux = recv( db->agents[ agent_index ].desc, &res, sizeof( REQ ), 0 );
            if( aux == -1 || aux == 0 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: connection to agent was lost during the transaction, request may have not been attended...\n" );
                break;
            }
            if( res.code != REQ_ACK ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: agent ack was not recieved, request may have not been attended...\n" );  
            }
            CLIENT_add_container( client, req->data, &db->agents[ agent_index ].addr );
            res.code = REQ_ACK;
            strcpy( res.data, "Success: container has been created...\n" );
            break;
        case REQ_STOP:
            cont_index = CLIENT_has_container( client, req->data );
            if( cont_index == -1 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: container was not found...\n" );
                break;
            }
            if( CONT_get_status( &client->db.containers[ cont_index ] ) == 0 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: container is already stopped...\n" );
                break;
            }
            agent_index = AGENTS_DB_find( db, &client->db.containers[ cont_index ].location );
            if( !AGENT_is_connected( &db->agents[ agent_index ] ) ){
                AGENT_connect( &db->agents[ agent_index ] );
            }
            if( !AGENT_is_connected( &db->agents[ agent_index ] ) ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: could not stablish connection to agent...\n" );
                break;
            }
            send( db->agents[ agent_index ].desc, req, sizeof( REQ ), 0 );
            aux = socket_timeout( db->agents[ agent_index ].desc, TIMEOUT );
            if( aux == -1 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: request may have not been attended...\n" );
                break;
            }
            if( aux == 0 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: agent response timedout, request may have not been attended...\n" );
                break;
            }
            aux = recv( db->agents[ agent_index ].desc, &res, sizeof( REQ ), 0 );
            if( aux == -1 || aux == 0 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: connection to agent was lost during the transaction, request may have not been attended...\n" );
                break;
            }
            CLIENT_stop_container( client, cont_index );
            res.code = REQ_ACK;
            strcpy( res.data, "Success: container has been stopped...\n" );
            break;
        case REQ_DELETE:
            cont_index = CLIENT_has_container( client, req->data );
            if( cont_index == -1 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: container was not found...\n" );
                break;
            }
            if( CONT_get_status( &client->db.containers[ cont_index ] ) == 1 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: can not remove container that is running...\n" );
                break;
            }
            agent_index = AGENTS_DB_find( db, &client->db.containers[ cont_index ].location );
            if( !AGENT_is_connected( &db->agents[ agent_index ] ) ){
                AGENT_connect( &db->agents[ agent_index ] );
            }
            if( !AGENT_is_connected( &db->agents[ agent_index ] ) ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: could not stablish connection to agent...\n" );
                break;
            }
            send( db->agents[ agent_index ].desc, req, sizeof( REQ ), 0 );
            aux = socket_timeout( db->agents[ agent_index ].desc, TIMEOUT );
            if( aux == -1 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: request may have not been attended...\n" );
                break;
            }
            if( aux == 0 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: agent response timedout, request may have not been attended...\n" );
                break;
            }
            aux = recv( db->agents[ agent_index ].desc, &res, sizeof( REQ ), 0 );
            if( aux == -1 || aux == 0 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: connection to agent was lost during the transaction, request may have not been attended...\n" );
                break;
            }
            CLIENT_delete_container( client, cont_index );
            res.code = REQ_ACK;
            strcpy( res.data, "Success: container has been deleted...\n" );
            break;
        case REQ_LIST:
            res.code = REQ_ACK;
            CLIENT_list_containers( client, res.data );
            send( client->desc, &res, sizeof( REQ ), 0 );
            break;
        default:
            res.code = REQ_ERROR;
            strcpy( res.data, "Error: request code is invalid...\n" );
            break;
    }
    send( client->desc, &res, sizeof( REQ ), 0 );
}

/* RUTINA DEL AC */
int ac_routine(){
    /* DECLARACION DE VARIABLES */
    SERVER ac_server;
    CLIENT client;
    int client_desc;
    struct sockaddr_in client_addr;
    int addr_size = sizeof( struct sockaddr_in );
    /* DECLARACION DE VARIABLES */
    printf( "AC: routine launched...\n" );
    if( initialize_tcp_server( &ac_server, AC_PORT ) == -1 ){
        printf( "AC: error on server initialization...\nAC: terminating execution...\n" );
        return -1;
    }
    printf( "AC: server is up...\n" );
    int status;
    while( 1 ){
//        printf( "EPA\n" );
        status = socket_timeout( ac_server.desc, 5000 ); // RECUPERAR 30 * 1000
        if( status == -1 ){
            printf( "AC (CRITICAL): error on poll() call...\n" );
        }
        else if( status == 0 ){
//            printf( "YAY\n" );
            AGENTS_DB_connect( db );
        }
        else{
            client_desc = accept( ac_server.desc, ( struct sockaddr * ) &client_addr, ( socklen_t * ) &addr_size );
            printf( "AC: client connection request recieved...\n" );
            if( client_desc == -1 ){
                printf( "AC (CRITICAL): error on accept() call...\n" );
            }
            else{
                printf( "AC: client connection accepted...\n" );
                REQ req;
                CLIENT_define( &client, client_desc, &client_addr );
                while( 1 ){
                    status = socket_timeout( client_desc, 5000 ); // RECUPERAR 30 * 1000
                    if( status == -1 ){
                        printf( "AGENT (CRITICAL): error on poll() call...\n" );
                    }
                    else if( status == 0 ){
                        AGENTS_DB_connect( db );
                    }
                    else{
                        status = recv( client.desc, &req, sizeof( REQ ), 0 );
                        if( status == -1 || status == 0 ){
                            close( client_desc );
                            break;
                        }
                        else{
                            printf( "AC: container request recieved...\n" );
                            attend( &client, &req );
                        }
                    }
                }
                printf( "AC: client connection lost...\nAC: awaiting new client connection...\n" );
            }
        }
    }
    close( ac_server.desc );
}


/* RUTINA DEL SH */
int sh_routine(){
    /* DECLARACION DE VARIABLES */
    SERVER sh_server;
    REQ req, res;
    int agent_desc;
    struct sockaddr_in agent_addr;
    int addr_size = sizeof( struct sockaddr_in );
    /* DECLARACION DE VARIABLES */
    printf( "SH: routine launched...\n" );
    if( initialize_tcp_server( &sh_server, SH_PORT ) == -1 ){
        printf( "SH: error on server initialization...\nAC: terminating execution...\n" );
        return -1;
    }
    printf( "SH: server is up...\n" );
    while( 1 ){
        //agent_desc = accept( sh_server.desc, ( struct sockaddr * ) &agent_addr, ( socklen_t * ) &addr_size );
        agent_desc = accept( sh_server.desc, NULL, NULL );
        if( agent_desc == -1 ){
            printf( "SH (CRITICAL): error on accept() call...\n" );
        }
        else{
            int status = socket_timeout( agent_desc, 10 * 1000 ); // 10 SEGUNDOS
            if( status == -1 ){
                printf( "SH (CRITICAL): error on poll() call...\n" );
            }
            else if( status > 0 ){
                status = recv( agent_desc, &req, sizeof( REQ ), 0 );
                if( status > 0 ){
                    if( req.code == REQ_CONNECT ){
                        printf( "SH: agent connection request recieved...\n" );
                        int i = AGENTS_DB_find( db, ( struct sockaddr_in * ) &req.data );
                        if( i == -1 ){
                            printf( "SH: agent is not registered in data base...\nSH: registering agent...\n" );
                            int i = AGENTS_DB_alloc( db );
                            if( i == -1 ){
                                res.code = REQ_ERROR;
                                strcpy( res.data, "Error: agent could not be allocated...\n" );
                                printf( "SH: error on agent allocation...\n\n" );
                            }
                            else{
                                res.code = REQ_ACK;
                                strcpy( res.data, "Success: agent has been registered...\n" );
                                AGENT_set_addr( &db->agents[ i ], ( struct sockaddr_in * ) &req.data );
                                printf( "SH: agent registered...\n" );
                            }
                        }
                        else{
                            printf( "SH: agent is already registered...\n" );
                            res.code = REQ_ACK;
                            //AGENT_set_addr( &db->agents[ i ], ( struct sockaddr_in * ) &req.data );
                            AGENT_set_desc( &db->agents[ i ], -1 );
                            strcpy( res.data, "Success: agent is already registered in data base...\n" );
                        }
                        send( agent_desc, &res, sizeof( REQ ), 0 );
                    }
                }
            }
            close( agent_desc );
        }
    }
    close( sh_server.desc );
}

int main(){
//    signal( SIGPIPE, SIG_IGN ); // OJO REVISAR, PUEDE QUE SEA NECESARIO
    int shm_id = shmget( IPC_PRIVATE, sizeof( AGENTS_DB ), IPC_CREAT | IPC_EXCL | S_IRWXU );
    if( shm_id == -1 ){
        printf( "MAIN (CRITICAL): error on shmget() call...\nMAIN: terminating execution...\n" );
        return -1;
    }
    db = shmat( shm_id, NULL, 0 );
    if( db == ( AGENTS_DB * ) -1 ){
        printf( "MAIN (CRITICAL): error on shmat() call...\nMAIN: terminating execution...\n" );
        return -1;
    }
    AGENTS_DB_init( db );
    int p_id = fork();
    if( p_id == -1 ){
        printf( "MAIN (CRITICAL): error on fork() call...\nMAIN: terminating execution...\n" );
        return -1;
    }
    else if( p_id ){
        ac_routine();
        shmdt( db );
        shmctl( shm_id, IPC_RMID, 0 );
    }
    else{
        sh_routine();
        shmdt( db );
        shmctl( shm_id, IPC_RMID, 0 );
    }
    AGENTS_DB_term( db );
    return 0;
}
