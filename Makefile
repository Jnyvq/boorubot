CFLAGS = -ansi -pedantic -Wall -Wundef -Wstrict-prototypes -std=c99 -Ofast -march=native
DEPS = -lcurl -ljansson -pthread
SRC=$(wildcard src/*.c)

boorubot: $(SRC) src/C-Thread-Pool/thpool.c
	$(CC) -o $@ $^ $(CFLAGS) $(DEPS) -D THREADED
