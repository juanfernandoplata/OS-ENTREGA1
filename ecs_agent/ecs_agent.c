#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>

#include "../lib/FORMATS/formats.h"

/* SE DEFINEN LOS PUERTOS DE LOS SERVIDORES **/
#define SH_PORT 9000
//#define AGENT_PORT 10000

#define MAX_ARGS 10
#define ARG_SIZE 24

#define TIMEOUT 250

/* ESTRUCTURA SERVIDOR PARA FACILITAR EL ORDEN */
typedef struct SERVER{
    int desc;
    struct sockaddr_in addr;
} SERVER;

int AGENT_PORT;

/* SE DECLARA GLOBALMENTE EL SERVIDOR DEL AGENTE, ESTO PARA NO TENER QUE PASARLO COMO ARGUMENTO CONSTANTEMENTE */
SERVER agent_server;

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
    if( listen( server->desc, 3 ) == -1 ){
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

/* ESTA FUNCION SE ENCARGA DE ATENDER LAS SOLICITUDES DEL ECS */
void attend(){
    /* DECLARACION DE VARIABLES */
    int ac_desc;
    struct sockaddr_in ac_addr;
    int addr_size = sizeof( struct sockaddr_in );
    /* DECLARACION DE VARIABLES */
    ac_desc = accept( agent_server.desc, ( struct sockaddr * ) &ac_addr, ( socklen_t * ) &addr_size );
    if( ac_desc == -1 ){
        return;
    }
    /* VARIABLES AUXILIARES */
    int aux;
    int exec;
    REQ req, res;
    char * args[ MAX_ARGS ];
    /* VARIABLES AUXILIARES */
    for( int i = 0; i < MAX_ARGS; i++ ){
        args[ i ] = malloc( ARG_SIZE );
    }
    strcpy( args[ 0 ], "docker" );
    printf( "ATTEND: routine launched...\n" );
    while( 1 ){
        aux = recv( ac_desc, &req, sizeof( REQ ), 0 );
        if( aux == -1 || aux == 0 ){
            close( ac_desc );
            return;
        }
        exec = 1;
        switch( req.code ){
            case REQ_PING:
                res.code = REQ_ACK;
                exec = 0;
                break;
            case REQ_CREATE:
                res.code = REQ_ACK;
                strcpy( res.data, "" );
                strcpy( args[ 1 ], "run" );
                strcpy( args[ 2 ], "-d" );
                strcpy( args[ 3 ], "--name" );
                strcpy( args[ 4 ], req.data );
                strcpy( args[ 5 ], &req.data[ 120 ] );
                args[ 6 ] = NULL;
                break;
            case REQ_STOP:
                res.code = REQ_ACK;
                strcpy( res.data, "" );
                strcpy( args[ 1 ], "stop" );
                strcpy( args[ 2 ], req.data );
                args[ 3 ] = NULL;
                break;
            case REQ_DELETE:
                res.code = REQ_ACK;
                strcpy( res.data, "" );
                strcpy( args[ 1 ], "rm" );
                strcpy( args[ 2 ], req.data );
                args[ 3 ] = NULL;
                break;
            default:
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: request code is invalid...\n" );
                exec = 0;
                break;
        }
        if( exec ){
            int p_id = fork(); // AQUI EL FORK PARA LLAMAR A EXEC CON EL COMANDO DE DOCKER EN CUESTION
            if( p_id == -1 ){
                res.code = REQ_ERROR;
                strcpy( res.data, "Error: fork() failed...\n" );
            }
            else if( p_id == 0 ){
                execvp( "docker", args );
            }
        }
        send( ac_desc, &res, sizeof( REQ ), 0 );
    }
}

/* ESTA FUNCION NOTIFICA AL SH DE LA CREACION DEL AGENTE. EN RECEPCION DE ESTA SOLICITUD, EL SH DEBE ESTABLECER CONEXION CON EL SERVIDOR DEL AGENTE */
int notify_sh( struct sockaddr_in * agent_server_addr ){
    int sh_desc = socket( AF_INET, SOCK_STREAM, 0 );
    if( sh_desc == -1 ){
        return -1;
    }
    struct sockaddr_in sh_addr;
    sh_addr.sin_family = AF_INET;
    sh_addr.sin_addr.s_addr = inet_addr( "127.0.0.1" );
    sh_addr.sin_port = htons( SH_PORT );
    int status = connect( sh_desc, ( struct sockaddr * ) &sh_addr, sizeof( struct sockaddr_in ) );
    if( status == -1 ){
        close( sh_desc );
        return -1;
    }
    REQ req, res;
    req.code = REQ_CONNECT;
    memcpy( req.data, agent_server_addr, sizeof( struct sockaddr_in ) );
    status = send( sh_desc, &req, sizeof( REQ ), 0 );
    if( status == -1 ){
        close( sh_desc );
        return -1;
    }
    status = socket_timeout( sh_desc, TIMEOUT );
    if( status == -1 || status == 0 ){
        close( sh_desc );
        return -1;
    }
    status = recv( sh_desc, &res, sizeof( REQ ), 0 );
    if( status == -1 ){
        close( sh_desc );
        return -1;
    }
    if( res.code != REQ_ACK ){
        close( sh_desc );
        return -1;
    }
    close( sh_desc );
    return 0;
}

/* RUTINA DEL AGENTE */
int agent_routine(){
    if( initialize_tcp_server( &agent_server, AGENT_PORT ) == -1 ){
        printf( "AGENT: error on server initialization...\nAGENT: terminatinf execution...\n" );
        return -1;
    }
    printf( "AGENT: server is up...\n" );
    while( 1 ){
        printf( "AGENT: (re)notifying sh...\n" );
        if( notify_sh( &agent_server.addr ) == -1 ){
            printf( "AGENT: sh could not be notified...\nAGENT: retrying in 10 seconds...\n" );
            sleep( 10 ); // 10 SEGUNDOS
        }
        else{
            printf( "AGENT: sh has been notified...\nAGENT: awaiting ac connection...\n" );
            int status = socket_timeout( agent_server.desc, 10 * 1000 ); // EL VALOR PUEDE DEBE SER ALTO (10 SEGUNDOS) PUES HAY UNA VERIFICACION EN DOS PASOS (EXPLICAR)
            if( status == -1 ){
                printf( "AGENT (CRITICAL): error on poll() call...\n" );
            }
            else if( status == 0 ){
                printf( "AGENT: connection was not requested (timeout)...\n" );
            }
            else{
                attend();
                printf( "AGENT: connection lost...\n" );
            }

        }
    }
    close( agent_server.desc );
    return 1;
}

int main( int argc, char * argv[] ){
    if( argc != 2 ){
        printf( "USAGE: ./ecs_agent PORT_NUM\n" );
    }
    else{
        AGENT_PORT = atoi( argv[ 1 ] );
        agent_routine();
    }
    return 0;
}




































// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <sys/poll.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <arpa/inet.h>
// #include <semaphore.h>
// #include <errno.h>
// #include <string.h>

// #include "../lib/FORMATS/formats.h"

// /* SE DEFINEN LOS PUERTOS DE LOS SERVIDORES **/
// #define SH_PORT 9000
// //#define AGENT_PORT 10000

// #define MAX_ARGS 10
// #define ARG_SIZE 24

// /* ESTRUCTURA SERVIDOR PARA FACILITAR EL ORDEN */
// typedef struct SERVER{
//     int desc;
//     struct sockaddr_in addr;
// } SERVER;

// int AGENT_PORT;

// /* SE DECLARA GLOBALMENTE EL SERVIDOR DEL AGENTE, ESTO PARA NO TENER QUE PASARLO COMO ARGUMENTO CONSTANTEMENTE */
// SERVER agent_server;

// /* ESTA FUNCION INICIALIZA UN SERVIDOR TCP EN EL PUERTO port. LA FUNCION RETORNA -1 SE SE PRESENTA UN ERROR */
// int initialize_tcp_server( SERVER * server, int port ){
//     server->desc = socket( AF_INET, SOCK_STREAM, 0 );
//     if( server->desc == -1 ){
//         return -1;
//     }
//     server->addr.sin_family = AF_INET;
//     server->addr.sin_addr.s_addr = INADDR_ANY;
//     server->addr.sin_port = htons( port );
//     if( bind( server->desc, ( struct sockaddr * ) &server->addr, sizeof( struct sockaddr_in ) ) == -1 ) {
//         return -1;
//     }
//     if( listen( server->desc, 3 ) == -1 ){
//         close( server->desc );
//         server->desc = -1;
//         return -1;
//     }
//     return server->desc;
// }

// /* ESTA FUNCION HACE POLLING SOBRE UN SOCKET PARA VER SI EN UN TIEMPO MENOR A TIMEOUT (EN MILISEGUNDOS) SE PUEE EJECUTAR ALGUNA OPERACION DE I/O */
// /* SOBRE EL SOCKET */
// int socket_timeout( int desc, int ms ){
//     struct pollfd target;
//     target.fd = desc;
//     target.events = POLLIN;
//     return poll( &target, 1, ms );
// }

// /* ESTA FUNCION SE ENCARGA DE ATENDER LAS SOLICITUDES DEL ECS */
// void attend(){
//     /* DECLARACION DE VARIABLES */
//     int ac_desc;
//     struct sockaddr_in ac_addr;
//     int addr_size = sizeof( struct sockaddr_in );
//     /* DECLARACION DE VARIABLES */
//     ac_desc = accept( agent_server.desc, ( struct sockaddr * ) &ac_addr, ( socklen_t * ) &addr_size );
//     if( ac_desc == -1 ){
//         return;
//     }
//     /* VARIABLES AUXILIARES */
//     int aux;
//     int exec;
//     REQ req, res;
//     char * args[ MAX_ARGS ];
//     /* VARIABLES AUXILIARES */
//     for( int i = 0; i < MAX_ARGS; i++ ){
//         args[ i ] = malloc( ARG_SIZE );
//     }
//     strcpy( args[ 0 ], "docker" );
//     printf( "ATTEND: routine launched...\n" );
//     while( 1 ){
//         aux = recv( ac_desc, &req, sizeof( REQ ), 0 );
//         if( aux == -1 || aux == 0 ){
//             close( ac_desc );
//             return;
//         }
// //        printf( "EPA\n" );
//         exec = 1;
//         switch( req.code ){
//             case REQ_PING:
//                 //printf( "Status: recieved ping request...\n" );
//                 res.code = REQ_ACK;
//                 exec = 0;
//                 break;
//             case REQ_CREATE:
//                 //printf( "ALGO QUIEREN CREAR\n" );
//                 res.code = REQ_ACK;
//                 strcpy( res.data, "" );
//                 strcpy( args[ 1 ], "run" );
//                 strcpy( args[ 2 ], "-d" );
//                 strcpy( args[ 3 ], "--name" );
//                 strcpy( args[ 4 ], req.data );
//                 strcpy( args[ 5 ], &req.data[ 120 ] );
//                 args[ 6 ] = NULL;
//                 break;
//             case REQ_STOP:
//                 res.code = REQ_ACK;
//                 strcpy( res.data, "" );
//                 strcpy( args[ 1 ], "stop" );
//                 strcpy( args[ 2 ], req.data );
//                 args[ 3 ] = NULL;
//                 break;
//             case REQ_DELETE:
//                 res.code = REQ_ACK;
//                 strcpy( res.data, "" );
//                 strcpy( args[ 1 ], "rm" );
//                 strcpy( args[ 2 ], req.data );
//                 args[ 3 ] = NULL;
//                 break;
//             default:
//                 res.code = REQ_ERROR;
//                 strcpy( res.data, "Error: request code is invalid...\n" );
//                 exec = 0;
//                 break;
//         }
//         if( exec ){
//             int p_id = fork(); // AQUI EL FORK PARA LLAMAR A EXEC CON EL COMANDO DE DOCKER EN CUESTION
//             if( p_id == -1 ){
//                 res.code = REQ_ERROR;
//                 strcpy( res.data, "Error: fork() failed...\n" );
//             }
//             else if( p_id == 0 ){
//                 execvp( "docker", args );
//             }
//         }
//         send( ac_desc, &res, sizeof( REQ ), 0 );
//     }
// }

// /* ESTA FUNCION NOTIFICA AL SH DE LA CREACION DEL AGENTE. EN RECEPCION DE ESTA SOLICITUD, EL SH DEBE ESTABLECER CONEXION CON EL SERVIDOR DEL AGENTE */
// int notify_sh( struct sockaddr_in * agent_server_addr ){
//     int sh_desc = socket( AF_INET, SOCK_STREAM, 0 );
//     if( sh_desc == -1 ){
//         return -1;
//     }
//     struct sockaddr_in sh_addr;
//     sh_addr.sin_family = AF_INET;
//     sh_addr.sin_addr.s_addr = inet_addr( "127.0.0.1" );
//     sh_addr.sin_port = htons( SH_PORT );
//     int status = connect( sh_desc, ( struct sockaddr * ) &sh_addr, sizeof( struct sockaddr_in ) );
//     if( status == -1 ){
//         close( sh_desc );
// //        printf( "EPA1\n" );
//         return -1;
//     }
//     REQ req, res;
//     req.code = REQ_CONNECT;
//     memcpy( req.data, agent_server_addr, sizeof( struct sockaddr_in ) );
//     status = send( sh_desc, &req, sizeof( REQ ), 0 );
//     if( status == -1 ){
//         close( sh_desc );
// //        printf( "EPA2\n" );
//         return -1;
//     }
//     status = socket_timeout( sh_desc, 10 * 1000 ); // 10 SEGUNDOS
//     if( status == -1 || status == 0 ){
//         close( sh_desc );
// //        printf( "EPA3\n" );
//         return -1;
//     }
//     status = recv( sh_desc, &res, sizeof( REQ ), 0 );
//     if( status == -1 ){
//         close( sh_desc );
// //        printf( "EPA4\n" );
//         return -1;
//     }
//     if( res.code != REQ_ACK ){
// //        printf( "EPA5\n" );
//         close( sh_desc );
//         return -1;
//     }
// //    printf( "ERRNO: %d\n", errno );
//     close( sh_desc );
//     return 0;
// }

// /* RUTINA DEL AGENTE */
// int agent_routine(){
//     if( initialize_tcp_server( &agent_server, AGENT_PORT ) == -1 ){
//         printf( "AGENT: server could not be initialized...\nAGENT: finishing execution...\n" );
//         return -1;
//     }
//     printf( "AGENT: server is up...\n" );
//     while( 1 ){ // PENSAR EN CONDICION DE SALIDA
//         printf( "AGENT: notifying sh...\n" );
//         if( notify_sh( &agent_server.addr ) == -1 ){
//             printf( "AGENT: sh could not be notified...\nAGENT: retrying in 60 seconds...\n" );
//             sleep( 5 ); // CAMBIAR
//         }
//         else{
//             printf( "AGENT: sh has been notified...\nAGENT: awaiting ac connection request...\n" );
//             int status = socket_timeout( agent_server.desc, 10 * 1000 );
//             if( status == -1 ){
//                 printf( "AGENT: poll() failed...\nAGENT: finishing execution...\n" );
//             }
//             else if( status == 0 ){
//                 printf( "AGENT: connection was not requested (timeout)...\n" );
//             }
//             else{
//                 printf( "AGENT: now attending server...\n" );
//                 attend();
//                 printf( "AGENT: connection lost...\n" );
//             }

//         }
//     }
//     close( agent_server.desc );
//     return 1;
// }

// int main( int argc, char * argv[] ){
//     if( argc != 2 ){
//         printf( "USAGE: ./ecs_agent PORT_NUM\n" );
//     }
//     else{
//         AGENT_PORT = atoi( argv[ 1 ] );
//         agent_routine();
//     }
//     return 0;
// }