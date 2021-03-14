CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CC = gcc
CFLAGS += -g -O2 -Werror -Wall 

SRC_FILES = $(wildcard *.c)

EXE_FILES = $(SRC_FILES:.c=)

all: $(EXE_FILES)
	echo $(EXE_FILES)

%/%.c:%.c $(DEPS_DIR)
	$(CC) $(CFLAGS) -c $@ $<

clean:
	rm -f $(EXE_FILES)

.PHONY: all clean
