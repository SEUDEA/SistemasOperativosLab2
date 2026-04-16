CC     = gcc
CFLAGS = -Wall -Wextra -Wno-unused-parameter -g

all: wish

wish: wish.c
	$(CC) $(CFLAGS) -o wish wish.c

clean:
	rm -f wish
