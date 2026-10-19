# C89 version of the PL/M-80 compiler and runtime: plmc-c
CC      ?= cc
CFLAGS  ?= -O2 -std=c89 -pedantic -Wall -Wextra
SRC     = util.c lex.c sym.c emit.c expr.c expr2.c stmt.c \
          decl.c vm.c main.c
OBJ     = $(SRC:.c=.o)

all: plmc-c

plmc-c: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c plm.h
	$(CC) $(CFLAGS) -c $<

lines:
	@awk 'length($$0)>71 {print FILENAME ": " FNR ": " length($$0)}' \
	  *.c *.h Makefile | grep . && exit 1 || echo "lines ok"

clean:
	rm -f plmc-c *.o

.PHONY: all lines clean
