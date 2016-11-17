CC=gcc
CCFLAGS=-g -std=gnu11 -I./src
LDFLAGS=-lao -lmpg123 -lwiringPi -lmicrohttpd -lpthread

TARGET=my123

SRC=$(wildcard src/**/*.c src/*.c)
OBJ=$(SRC:%.c=%.o)

OBJWITHOUTMAIN := $(filter-out src/main.o,$(OBJ))

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CCFLAGS) -o $(TARGET) $^ $(LDFLAGS)

# To obtain object files
%.o: %.c
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f $(TARGET) test $(OBJ) $(TESTOBJ)
