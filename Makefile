CC = gcc
CFLAGS = -Wall
TARGETS = server client

all: $(TARGETS)

server: server.c common.h
	$(CC) $(CFLAGS) server.c -o server

client: client.c common.h
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f $(TARGETS)