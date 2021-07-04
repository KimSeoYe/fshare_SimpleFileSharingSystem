all: socket.h socket.c fshared.c fshare.c
	gcc -o fshared fshared.c socket.c -pthread
	gcc -o fshare fshare.c socket.c

fshared: socket.h socket.c fshared.c
	gcc -o fshared fshared.c socket.c -pthread

fshare: socket.h socket.c fshare.c
	gcc -o fshare fshare.c socket.c

clean:
	rm -rf fshared fshare socket.o