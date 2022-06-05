from ecs_client import ecs_client
from time import sleep

def main():
    client = ecs_client.ecs_client()

    client.connect()

    for i in range( 1, 12 ):
        name = "wp" + str( i )
        client.create_instance( name, "wordpress" )
        sleep( 1 )
    
    for i in range( 1, 11 ):
        name = "wp" + str( i )
        client.stop_instance( name )
        sleep( 1 )

    for i in range( 6, 11 ):
        name = "wp" + str( i )
        client.erase_instance( name )
        sleep( 1 )
    
    client.list_instances()

    client.disconnect()

main()