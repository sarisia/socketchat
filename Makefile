.PHONY: server
server:
	gcc -pthread -o server llist.c server.c

.PHONY: client
client:
	gcc -pthread -o client client.c
