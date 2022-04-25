CFLAGS = -Wall -Wextra -pedantic -ggdb3

PREFIX = /usr/local

SRC = trash.c util.c
OBJ = $(SRC:.c=.o)
BIN = tdo

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

install: all
	install -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755  $(BIN) $(DESTDIR)$(PREFIX)/bin

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	$(RM) $(OBJ) $(BIN)

.PHONY: all install uninstall clean
