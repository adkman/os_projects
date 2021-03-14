CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CC = gcc
CFLAGS += -g -O2 -Werror -Wall -lpthread
LDFLAGS += -pthread

SRC_FILES = $(wildcard *.c)

EXE_FILES = $(SRC_FILES:.c=)

all: $(EXE_FILES)
	echo $(EXE_FILES)

%/%.c: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c $@ $<

clean:
	rm -f $(EXE_FILES)

.PHONY: all clean
