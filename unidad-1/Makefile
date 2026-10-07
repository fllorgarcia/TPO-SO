# Makefile - compila el auditor a partir de todos los módulos
CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu11
OBJS    = main.o procfs.o shells.o procs.o syscalls.o logger.o tracer.o

auditor: $(OBJS)
	$(CC) $(CFLAGS) -o auditor $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) auditor

.PHONY: clean
