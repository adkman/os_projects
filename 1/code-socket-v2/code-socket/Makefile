## [M1]
#  Explain the following here
#
# Here, the lastword in a MAKEFILE_LIST is the current Makefile.
# realpath will give the absolute path of the Makefile.
# shell dirname will give the directory of the Makefile.
# Hence CUR_DIR (simply expanded variable) is set with the 
# absolute path of the Makefile's directory.
#  
CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

## [M2]
#  Explain the following here
#  ...
# Here there are 3 recursively expanded variables
# CC is set with value gcc (which will be our compiler)
# CFLAGS is set with the gcc options
# -g produces debugging information
# -O2 sets the optimization level
# -Werror makes all warnings into error which means the compilation will 
# fail even if there are warnings
# -Wall turns on many warning flags
CC = gcc
CFLAGS += -g -O2 -Werror -Wall
LDFLAGS +=

## [M3] -- REMOVED
#  ...

## [M5]
#  Explain the following here
#  ...
# This wildcard function will store all the .c source files in SRC_FILES var
#
SRC_FILES = $(wildcard *.c)

## [M6]
#  Explain the following here
#  ...
# Creates a list of executable files removing the .c from SRC_FILES and storing it
# into EXE_FILES variable (for e.g. client.c -> client and server.c -> server)
# (The .c is replaced with nothing in the SRC_FILES variable data)
EXE_FILES = $(SRC_FILES:.c=)

## [M7]
#  Explain the following here
#  ...
# This defines an all (default) target with prerequisites as the executable files
# which are defined as targets below
all: $(EXE_FILES)

## [M8] -- UPDATED
#  Explain the following here
#  ...
# This defines target for the executables with prerequisites as their .c source
# files. Then the recipe uses the previously defined variables to compile the .c
# source files.
# Here the recipe will only be executed if the target file is older or missing than
# the corresponding .c source files.
# $@ variable substitutes itself with the target name
# $< variable substitutes itself with the source file name
# So that the executables created will have the same name as the target.
%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< 

## [M9]
#  Explain the following here
#  ...
# Here the target is clean and recipe is to remove all the 
# EXE_FILES (executables produced by gcc).
# Hence when we execute make clean, it will remove all the generated executable files
# from the project.
clean:
	rm -f $(EXE_FILES)

## [M10]
#  Explain the following here
#  ...
# Here the targets all and clean are declared as prerequisites to .PHONY target
# to make sure that those targets will run as expected even if there is any file
# named all or clean in the directory. This basically tells make that the targets
# all and clean are not associated to any file in the project.
.PHONY: all clean
