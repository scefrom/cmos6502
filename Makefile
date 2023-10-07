CC=gcc

CFLAGS=-Wall -O3 -fPIC -rdynamic

SRC=./src
SRC_WC=$(wildcard $(SRC)/*.c)

BIN=c6502



.PHONY: all clean cleanall

all: $(BIN)

clean:
	rm -f $(SRC)/*.o

cleanall: clean
	rm -f $(BIN)



$(BIN): $(patsubst %.c,%.o,$(SRC_WC))
	$(CC) $(CFLAGS) -o $(BIN) $^

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $<