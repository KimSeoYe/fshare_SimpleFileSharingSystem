all: server.c client.c
	gcc -o server server.c -pthread
	gcc -o client client.c 

server: server.c
	gcc -o server server.c -pthread

client: client.c
	gcc -o client client.c 

clean:
	rm -rf server client