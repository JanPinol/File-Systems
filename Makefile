CC=gcc
CFLAGS=-Iinclude -Wall -Wextra -Wpedantic -Werror
LDFLAGS=
SRC=$(wildcard src/*.c)
OBJ=$(patsubst src/%.c,obj/%.o,$(SRC))
DEPS=$(wildcard include/*.h)

all: main

main: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o fsutils

obj/%.o: src/%.c $(DEPS)
	mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj fsutils
