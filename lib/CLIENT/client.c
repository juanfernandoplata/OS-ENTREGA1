#include "./client.h"

void CONT_define( CONT * this, char * name, struct sockaddr_in * location ){
    strcpy( this->name, name );
    this->status = 1;
    this->location = * location;
}

int CONT_get_status( CONT * this ){
    return this->status;
}

void CONTS_DB_init( CONTS_DB * this ){
    for( int i = 0; i < MAX_CONTS_PER_CLIENT; i++ ){
        this->containers[ i ].status = -1;
    }
}

int CONTS_DB_alloc( CONTS_DB * this ){
    int i = MAX_CONTS_PER_CLIENT - 1;
    while( i >= 0 ){
        if( this->containers[ i ].status == -1 ){
            break;
        }
        i--;
    }
    return i;
}

void CLIENT_define( CLIENT * this, int desc, struct sockaddr_in * addr ){
    this->desc = desc;
    this->addr = * addr;
    CONTS_DB_init( &this->db );
}

int CLIENT_has_container( CLIENT * this, char * name ){
    int i = MAX_CONTS_PER_CLIENT - 1;
    while( i >= 0 ){
        if( this->db.containers[ i ].status > -1 && !strcmp( this->db.containers[ i ].name, name ) ){
            break;
        }
        i--;
    }
    return i;
}

int CLIENT_add_container( CLIENT * this, char * name, struct sockaddr_in * location ){
    int i = CONTS_DB_alloc( &this->db );
    if( i == -1 ){
        return -1;
    }
    CONT_define( &this->db.containers[ i ], name, location );
}

void CLIENT_stop_container( CLIENT * this, int index ){
    this->db.containers[ index ].status = 0;
}

void CLIENT_delete_container( CLIENT * this, int index ){
    this->db.containers[ index ].status = -1;
}

void CLIENT_list_containers( CLIENT * this, char * listing ){
    strcpy( listing, "" );
    for( int i = 0; i < MAX_CONTS_PER_CLIENT; i++ ){
        if( this->db.containers[ i ].status > -1 ){
            strcat( listing, this->db.containers[ i ].name );
            if( this->db.containers[ i ].status == 0 ){
                strcat( listing, " (stopped)" );
            }
            else{
                strcat( listing, " (running)" );
            }
            strcat( listing, "\n" );
        }
    }
}

// void CONT_LIST_print( CONT_LIST * this ){
//     CONT_LIST_NODE * node = this->first;
//     printf( "START OF PRINT\n" );
//     while( node ){
//         printf( "NAME: %s\n", node->cont.name );
//         node = node->next;
//     }
//     printf( "END OF PRINT\n" );
// }