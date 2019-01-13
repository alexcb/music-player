CC=gcc -g
CCFLAGS=`pkg-config --cflags gtk+-3.0` -Werror
LDFLAGS=`pkg-config --libs gtk+-3.0 libcurl json-c`

SRC=$(wildcard *.c)
OBJ=$(SRC:%.c=%.o)

build: hello

hello: $(OBJ)
	$(CC) $(CCFLAGS) -o hello $^ $(LDFLAGS)

# To obtain object files
%.o: %.c
	$(CC) -c $(CCFLAGS) $< -o $@

install: hello
	cp hello /home/alex/music-ui

run: hello
	gdb -ex=r --args hello localhost:8080 music:80

clean:
	rm -f hello $(OBJ)
