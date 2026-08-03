CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -Isrc

SRCS = $(wildcard src/*.c)
OUT = build/micro

$(OUT): $(SRCS) src/micro.h src/config.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $(OUT)

all: $(OUT)

clean:
	rm -rf build
