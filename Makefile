.POSIX:

.PHONY: third_party format tidy clean

include common.mk

BIN = client

XCFLAGS = \
	$(CFLAGS_COMMON) -Wcast-qual -Wconversion -Wc++-compat -Wpointer-arith \
	-Wunused-macros -Wredundant-decls

LDLIBS = # -llmdb `pkg-config --libs libcurl`

INCLUDES = -isystem third_party/termbox/src

OBJ = \
	src/buffer.o \
	src/input.o

all: release

.c.o:
	$(CC) $(XCFLAGS) $(INCLUDES) -c $< -o $@

# Makefile pattern matching isn't portable so we must use this hack :(
third_party:
	$(MAKE) -f third_party.mk

$(BIN): $(OBJ) third_party
	$(CC) $(XCFLAGS) -o $@ $(OBJ) $(THIRD_PARTY_OBJ) $(LDLIBS) $(LDFLAGS)

release:
	$(MAKE) $(BIN) CFLAGS="$(CFLAGS) -DNDEBUG"

sanitize:
	$(MAKE) $(BIN) CFLAGS="$(CFLAGS) -fsanitize=address,undefined"

format:
	clang-format -i src/*.c

tidy:
	clang-tidy src/*.c -- $(XCFLAGS) $(INCLUDES)

clean:
	rm -f $(BIN) $(OBJ)
	$(MAKE) -f third_party.mk clean
