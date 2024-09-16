CC=gcc
CFLAGS=-I.
DEPS = client.h server.h
OBJ = client.o server.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

client: client.o
	$(CC) -o $@ $^ $(CFLAGS)

server: server.o
	$(CC) -o $@$^ $(CFLAGS)