#include "./agent.h"

void AGENT_set_desc( AGENT * this, int desc ){
    this->desc = desc;
}

void AGENT_set_addr( AGENT * this, struct sockaddr_in * addr ){
    this->desc = -1;
    this->addr = * addr;
}

int AGENT_is_connected( AGENT * this ){
    return this->desc > -1;
}

int AGENT_match( AGENT * this, struct sockaddr_in * addr ){
    return this->addr.sin_family == addr->sin_family &&
           this->addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
           this->addr.sin_port == addr->sin_port;
}

int AGENT_connect( AGENT * this ){
    int status;
    this->desc = socket( AF_INET, SOCK_STREAM, 0 );
    if( this->desc == -1 ){
        printf( "AGENT LIB (CRITICAL): error on socket() call...\n" );
        return -1;
    }
    int tries = 0;
    status = -1;
    while( tries < MAX_TRIES && status == -1 ){
        status = connect( this->desc, ( struct sockaddr * ) &this->addr, sizeof( struct sockaddr_in ) );
        tries++;
    }
    if( status == -1 ){
        close( this->desc );
        this->desc = -1;
    }
    return status;
}

void AGENTS_DB_init( AGENTS_DB * this ){
    for( int i = 0; i < MAX_AGENTS; i++ ){
        this->agents[ i ].desc = -2;
        this->agents[ i ].containers = 0;
    }
}

int AGENTS_DB_alloc( AGENTS_DB * this ){
    int i = MAX_AGENTS - 1;
    while( i >= 0 ){
        if( this->agents[ i ].desc == -2 ){
            break;
        }
        i--;
    }
    return i;
}

int AGENTS_DB_select( AGENTS_DB * this ){
    int i = -1;
    int low = MAX_CONTS_PER_AGENT;
    for( int j = 0; j < MAX_AGENTS; j++ ){
        if( this->agents[ j ].desc > -1 && this->agents[ j ].containers < low ){
            i = j;
            low = this->agents[ j ].containers;
        }
    }
    this->agents[ i ].containers++;
    return i;
}

int AGENTS_DB_find( AGENTS_DB * this, struct sockaddr_in * addr ){
    int i = MAX_AGENTS - 1;
    while( i >= 0 ){
        if( AGENT_match( &this->agents[ i ], addr ) ){
            break;
        }
        i--;
    }
    return i;
}

void AGENTS_DB_connect( AGENTS_DB * this ){
    for( int i = 0; i < MAX_AGENTS; i++ ){
        if( this->agents[ i ].desc == -1 ){
            AGENT_connect( &this->agents[ i ] );
        }
    }
}

void AGENTS_DB_term( AGENTS_DB * this ){
    for( int i = 0; i < MAX_AGENTS; i++ ){
        if( this->agents[ i ].desc > -1 ){
            close( this->agents[ i ].desc );
        }
    }
}
