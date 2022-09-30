CFLAGS = -Wall -Wextra -pedantic -ggdb3

PREFIX = /usr/local

BIN = lstrash mvtrash rmtrash untrash
SRC = $(BIN:=.c) trash.c util.c
OBJ = $(SRC:.c=.o)

all: $(BIN)

TRASH = trash.o

lstrash: $(TRASH) util.o lstrash.o
	$(CC) $(LDFLAGS) -o $@ $^

mvtrash: $(TRASH) util.o mvtrash.o
	$(CC) $(LDFLAGS) -o $@ $^

rmtrash: $(TRASH) util.o rmtrash.o
	$(CC) $(LDFLAGS) -o $@ $^

untrash: $(TRASH) util.o untrash.o
	$(CC) $(LDFLAGS) -o $@ $^

install: all
	install -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755  $(BIN) $(DESTDIR)$(PREFIX)/bin

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	$(RM) $(OBJ) $(BIN)

.PHONY: all install uninstall clean
