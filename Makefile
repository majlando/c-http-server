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
test: tests-all
	@echo "Running tests..."
	@for t in $(TEST_BINS); do \
		./$$t || exit 1; \
	done

# Build each test binary from its own .c plus src/http_parser.o
tests/test_http_parser: tests/test_http_parser.o src/http_parser.o | bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_http_parser_fragmented: tests/test_http_parser_fragmented.o src/http_parser.o | bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Build all test binaries found under tests/*.c -> tests/<name>
TEST_BINS = $(TEST_SRC:.c=)

tests-all: $(TEST_BINS) | bin

.PHONY: test tests-all

.PHONY: asan
asan:
	@echo "Building and running tests with AddressSanitizer and UBSan..."
	make clean
	CFLAGS="-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -I./src" make -j
	make test

.PHONY: integration-test
integration-test: $(BIN)
	@echo "Running integration test..."
	@tests/integration/test_server.sh

tests/%: tests/%.c src/http_parser.o src/fsutils.o | bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

