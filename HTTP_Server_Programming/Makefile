CC=g++
CFLAGS= -c -g 
LFLAGS= -pthread -lstdc++

all: server clean

server: server.o
	$(CC) $(LFLAGS) server.o -o server

server.o: 
	$(CC) $(CFLAGS) server.cpp

clean:
	rm -rf *.o
