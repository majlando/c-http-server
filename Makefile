# Simple Makefile for starter C HTTP server
CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2
LDFLAGS ?=

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

TEST_SRC = $(wildcard tests/*.c)
TEST_OBJ = $(TEST_SRC:.c=.o)


BIN = bin/c-http-server

all: $(BIN)

$(BIN): $(OBJ) | bin
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

bin:
	mkdir -p bin

clean:
	rm -rf bin $(OBJ)

.PHONY: all clean

test: CFLAGS += -I./src
test: tests/test_http_parser

tests/test_http_parser: $(TEST_OBJ) $(filter-out src/main.o, $(OBJ)) | bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

