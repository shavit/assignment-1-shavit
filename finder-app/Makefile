# CC = $(or $(CROSS_COMPILE), gcc)
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld


.PHONY: build
build:
	$(CC) -g -Wall -o writer writer.c

clean:
	rm -f writer
	rm -f *.o
