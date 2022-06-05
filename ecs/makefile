out: agent client elastic
	gcc -o ecs ecs.o client.o agent.o

elastic:
	gcc -c ecs.c ../lib/CLIENT/client.h ../lib/AGENT/agent.h ../lib/FORMATS/formats.h

client:
	gcc -c ../lib/CLIENT/client.c ../lib/CLIENT/client.h

agent:
	gcc -c ../lib/AGENT/agent.c ../lib/AGENT/agent.h ../lib/FORMATS/formats.h