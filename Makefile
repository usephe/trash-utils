CFLAGS = -Wall -Wextra -pedantic -ggdb3

SRC = stm.c util.c
OBJ = $(SRC:.c=.o);

stm: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

clean:
	$(RM) stm $(OBJ)

.PHONY: all clean
