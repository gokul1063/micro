CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

micro: micro.c config.c
	$(CC) $(CFLAGS) micro.c config.c -o micro

clean:
	rm -f micro
