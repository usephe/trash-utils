CFLAGS = -Wall -Wextra -pedantic -ggdb3

SRC = stm.c util.c
OBJ = $(SRC:.c=.o);
BIN = stm

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

run:
	@$(MAKE) -s exec
runv:
	@$(MAKE) -s valgrind_exec

exec: stm
	@./stm stm

valgrind_exec: stm
	@valgrind --track-fds=yes ./$(BIN) $(BIN)

clean:
	$(RM) $(OBJ) $(BIN)

.PHONY: run runv clean
