CC=gcc

default: build
all: build

.PHONEY: build
build:
	${CC} -g -Wall -Werror aesdsocket.c -o aesdsocket

clean:
	exit 0;

run: all
	./aesdsocket

stop_daemon:
	@ pgrep aesdsocket | xargs -r kill -SIGTERM
