CC=gcc
CCFLAGS=-std=gnu11 -Wall -Werror -I./src `./use-pi-def.sh` -DDEBUG_BUILD=1
LDFLAGS=-lmpg123 -lmicrohttpd -lpthread -lm -ljson-c -lttspico -lssl -lcrypto `./use-pi-lib.sh`

SRC=$(wildcard src/**/*.c src/*.c)
OBJ=$(SRC:%.c=%.o)

TESTSRC=$(wildcard tests/**/*.c tests/*.c)
TESTOBJ=$(TESTSRC:%.c=%.o)

OBJWITHOUTMAIN := $(filter-out src/main.o src/check.o,$(OBJ))

build: music-server music-check test

music-server: $(OBJWITHOUTMAIN) src/main.o
	$(CC) $(CCFLAGS) -o music-server $^ $(LDFLAGS)

music-check: $(OBJWITHOUTMAIN) src/check.o
	$(CC) $(CCFLAGS) -o music-check $^ $(LDFLAGS)

test: $(OBJWITHOUTMAIN) $(TESTOBJ)
	$(CC) $(CCFLAGS) -o test $^ $(LDFLAGS)

.PHONY: reformat
reformat:
	find -regex '.*/.*\.\(c\|h\)$$' -exec clang-format-7 -i {} \;

.PHONY: run
run: music-server
	HTTP_PORT=7777 ./music-server ~/music ~/streams ~/playlists ~/music-id3-cache
	#./music-server /media/nugget_share/music/alex-beet ~/streams ~/playlists ~/music-id3-cache

.PHONY: random
random:
	./random-playlist --host localhost:7777

# To obtain object files
%.o: %.c
	$(CC) -c $(CCFLAGS) $< -o $@

clean:
	rm -f music-server music-check test $(OBJ) $(TESTOBJ)
