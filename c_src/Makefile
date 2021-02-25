CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O3 -Iinclude
AR ?= ar

SRC = src/plinth_crc32c.c src/plinth_vector.c src/plinth_client.c
OBJ = $(SRC:.c=.o)
LIB = libplinth.a

all: $(LIB) test_client

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test_client: tests/test_client.c $(LIB)
	$(CC) $(CFLAGS) tests/test_client.c $(LIB) -o $@

test: test_client
	./test_client

clean:
	rm -f src/*.o $(LIB) test_client

.PHONY: all test clean
