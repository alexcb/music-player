CC=gcc
CCFLAGS=-g -std=gnu11 -Wall -Werror -I./src `./use-pi-def.sh`
LDFLAGS=-lasound -lpulse -lpulse-simple -lmpg123 -lmicrohttpd -lpthread -lm -ljson-c -lssl -lcrypto `./use-pi-lib.sh`

SRC=$(wildcard src/**/*.c src/*.c)
OBJ=$(SRC:%.c=%.o)

TESTSRC=$(wildcard tests/**/*.c tests/*.c)
TESTOBJ=$(TESTSRC:%.c=%.o)

OBJWITHOUTMAIN := $(filter-out src/main.o,$(OBJ))

build: my123 test

my123: $(OBJ)
	$(CC) $(CCFLAGS) -o my123 $^ $(LDFLAGS)

test: $(OBJWITHOUTMAIN) $(TESTOBJ)
	$(CC) $(CCFLAGS) -o test $^ $(LDFLAGS)

# To obtain object files
%.o: %.c
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f my123 test $(OBJ) $(TESTOBJ)
