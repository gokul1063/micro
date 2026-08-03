CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

SRCS = main.c terminal.c buffer.c highlight.c fileio.c tabs.c find.c input.c screen.c config.c

micro: $(SRCS) micro.h
	$(CC) $(CFLAGS) $(SRCS) -o micro

clean:
	rm -f micro
